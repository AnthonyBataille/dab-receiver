#include "iq_diagnostics.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

namespace dab {
namespace {

void check_output(const std::ostream& output) {
    if (!output) {
        throw std::runtime_error("Error writing diagnostic CSV");
    }
}

// A uniquely reserved sibling directory keeps staging on the destination volume.
// Never truncate a pre-existing destination (including the input recording).
class PendingCsv {
  public:
    explicit PendingCsv(const std::filesystem::path& destination) : destination_(destination) {
        if (destination.empty() || std::filesystem::exists(destination)) {
            throw std::runtime_error("CSV destination must be a new file: " + destination.string());
        }
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            directory_ = destination;
            directory_ += ".tmp-" + std::to_string(stamp) + "-" + std::to_string(attempt);
            if (std::filesystem::create_directory(directory_)) {
                break;
            }
            directory_.clear();
        }
        if (directory_.empty()) {
            throw std::runtime_error("Cannot reserve temporary CSV for: " + destination.string());
        }
        temporary_ = directory_ / "output.csv";
        stream_.open(temporary_);
        if (!stream_) {
            cleanup();
            throw std::runtime_error("Cannot open temporary CSV for: " + destination.string());
        }
    }

    ~PendingCsv() {
        cleanup();
    }

    std::ostream& stream() {
        return stream_;
    }

    void close() {
        stream_.flush();
        check_output(stream_);
        stream_.close();
        check_output(stream_);
    }

    void publish() {
        if (std::filesystem::exists(destination_)) {
            throw std::runtime_error("CSV destination already exists: " + destination_.string());
        }
        std::filesystem::rename(temporary_, destination_);
        published_ = true;
    }

    void commit() {
        committed_ = true;
    }

  private:
    void cleanup() noexcept {
        stream_.close();
        std::error_code ignored;
        if (published_ && !committed_) {
            std::filesystem::remove(destination_, ignored);
        }
        if (!temporary_.empty()) {
            std::filesystem::remove(temporary_, ignored);
        }
        if (!directory_.empty()) {
            std::filesystem::remove(directory_, ignored);
        }
    }

    std::filesystem::path destination_, directory_, temporary_;
    std::ofstream stream_;
    bool published_ = false;
    bool committed_ = false;
};

} // namespace

IqDiagnostics::IqDiagnostics(std::uint64_t sample_rate, std::ostream* power, std::ostream* spectrum)
    : sample_rate_(sample_rate), power_(power), spectrum_(spectrum) {
    if (sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }
    for (auto* output : {power_, spectrum_}) {
        if (output) {
            output->imbue(std::locale::classic());
            *output << std::defaultfloat << std::setprecision(17);
        }
    }
    if (power_) {
        *power_ << "window_start_seconds,mean_linear_power,sample_count\n";
        check_output(*power_);
    }
    if (spectrum_) {
        *spectrum_ << "relative_frequency_hz,linear_bin_power\n";
        check_output(*spectrum_);
    }
}

void IqDiagnostics::emit_power() {
    *power_ << static_cast<double>(sample_index_ - power_count_) / static_cast<double>(sample_rate_)
            << ',' << power_sum_ / static_cast<double>(power_count_) << ',' << power_count_ << '\n';
    check_output(*power_);
    power_sum_ = 0;
    power_count_ = 0;
}

void IqDiagnostics::consume(std::span<const std::complex<float>> samples) {
    for (const auto sample : samples) {
        ++sample_index_;
        if (power_) {
            const double i = sample.real();
            const double q = sample.imag();
            power_sum_ += i * i + q * q;
            if (++power_count_ == power_window_size) {
                emit_power();
            }
        }
        if (spectrum_ && spectrum_count_ < spectrum_size) {
            spectrum_samples_[spectrum_count_++] = sample;
        }
    }
}

void IqDiagnostics::finish() {
    if (spectrum_ && spectrum_count_ != spectrum_size) {
        throw std::runtime_error("Spectrum requires at least 2048 complex samples; received " +
                                 std::to_string(spectrum_count_));
    }
    if (power_ && power_count_ != 0) {
        emit_power();
    }
    if (spectrum_) {
        // Provisional rectangular-window direct DFT, NOT the Phase 5 production FFT.
        // Forward sign: exp(-j*2*pi*k*n/N); shifted order: -N/2 ... N/2-1.
        for (std::size_t shifted = 0; shifted < spectrum_size; ++shifted) {
            const auto k = (shifted + spectrum_size / 2) % spectrum_size;
            std::complex<double> sum{};
            for (std::size_t n = 0; n < spectrum_size; ++n) {
                const double angle = -2.0 * std::numbers::pi * static_cast<double>(k * n) /
                                     static_cast<double>(spectrum_size);
                sum +=
                    spectrum_samples_[n] * std::complex<double>(std::cos(angle), std::sin(angle));
            }
            const double frequency =
                (static_cast<double>(shifted) - static_cast<double>(spectrum_size / 2)) *
                (static_cast<double>(sample_rate_) / static_cast<double>(spectrum_size));
            *spectrum_ << frequency << ','
                       << std::norm(sum) / static_cast<double>(spectrum_size * spectrum_size)
                       << '\n';
            check_output(*spectrum_);
        }
    }
}

IqSummary export_iq_diagnostics(const std::filesystem::path& input, std::uint64_t sample_rate,
                                std::ostream& summary,
                                const std::optional<std::filesystem::path>& power_path,
                                const std::optional<std::filesystem::path>& spectrum_path,
                                std::size_t block_capacity) {
    std::unique_ptr<PendingCsv> power, spectrum;
    if (power_path) {
        power = std::make_unique<PendingCsv>(*power_path);
    }
    if (spectrum_path) {
        spectrum = std::make_unique<PendingCsv>(*spectrum_path);
    }
    IqDiagnostics diagnostics(sample_rate, power ? &power->stream() : nullptr,
                              spectrum ? &spectrum->stream() : nullptr);
    const auto result =
        summarize_iq_file(input, sample_rate, summary, &diagnostics, block_capacity);
    for (auto* csv : {power.get(), spectrum.get()}) {
        if (csv) {
            csv->close();
        }
    }
    for (auto* csv : {power.get(), spectrum.get()}) {
        if (csv) {
            csv->publish();
        }
    }
    for (auto* csv : {power.get(), spectrum.get()}) {
        if (csv) {
            csv->commit();
        }
    }
    return result;
}

} // namespace dab

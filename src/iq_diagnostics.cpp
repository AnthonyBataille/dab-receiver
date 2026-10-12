#include "iq_diagnostics.hpp"

#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <locale>
#include <numbers>
#include <stdexcept>

namespace dab {

void check_output(const std::ostream& output) {
    if (!output) {
        throw std::runtime_error("Error writing diagnostic CSV");
    }
}

void IqDiagnostics::initialize_() {
    if (sample_rate_ == 0) {
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

IqDiagnostics::IqDiagnostics(std::uint64_t sample_rate, std::ostream* power, std::ostream* spectrum)
    : sample_rate_(sample_rate), power_(power), spectrum_(spectrum) {
    initialize_();
}

void IqDiagnostics::emit_power_() {
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
                emit_power_();
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
        emit_power_();
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

} // namespace dab

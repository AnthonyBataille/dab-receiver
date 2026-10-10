#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <fstream>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <span>

namespace dab {

// Stage beside the destination so publication stays on the same volume.
class PendingCsv {
  public:
    PendingCsv(const std::filesystem::path& destination);

    ~PendingCsv();

    std::ostream& stream();

    void close();

    void publish();

    void commit();

  private:
    void cleanup() noexcept;

    std::filesystem::path destination_, temporary_, backup_;
    std::ofstream stream_;
    bool backed_up_ = false;
    bool published_ = false;
    bool committed_ = false;
};

// Streaming CSV sink. State and summation order are independent of reader blocks.
class IqDiagnostics {
  public:
    IqDiagnostics(std::uint64_t sample_rate, std::ostream* power, std::ostream* spectrum);
    void consume(std::span<const std::complex<float>> samples);
    void finish();
    void print_iq_summary();
    void set_summary_info(const std::uint64_t num_complex_samples, const double duration_seconds);
    // Destinations may exist. Old CSVs are preserved if processing fails.
    void export_iq_diagnostics(PendingCsv* power_csv, PendingCsv* spectrum_csv);

  private:
    static constexpr std::size_t power_window_size = 512;
    static constexpr std::size_t spectrum_size = 2048;
    void initialize_();
    void emit_power_();
    std::uint64_t sample_rate_;
    std::uint64_t num_complex_samples_;
    double duration_seconds_;
    std::ostream* power_;
    std::ostream* spectrum_;
    std::uint64_t sample_index_ = 0;
    double power_sum_ = 0;
    std::size_t power_count_ = 0;
    std::array<std::complex<double>, spectrum_size> spectrum_samples_{};
    std::size_t spectrum_count_ = 0;
};

} // namespace dab

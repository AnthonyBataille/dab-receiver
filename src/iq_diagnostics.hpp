#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>

namespace dab {

// Streaming CSV sink. State and summation order are independent of reader blocks.
class IqDiagnostics {
  public:
    IqDiagnostics(std::uint64_t sample_rate, std::ostream* power, std::ostream* spectrum);
    void consume(std::span<const std::complex<float>> samples);
    void finish();

  private:
    static constexpr std::size_t power_window_size = 512;
    static constexpr std::size_t spectrum_size = 2048;
    void initialize_();
    void emit_power_();
    std::uint64_t sample_rate_;
    std::ostream* power_;
    std::ostream* spectrum_;
    std::uint64_t sample_index_ = 0;
    double power_sum_ = 0;
    std::size_t power_count_ = 0;
    std::array<std::complex<double>, spectrum_size> spectrum_samples_{};
    std::size_t spectrum_count_ = 0;
};

} // namespace dab

#pragma once

#include "iq_file_reader.hpp"
#include "iq_summary.hpp"

#include <array>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>

namespace dab {

inline constexpr std::size_t power_window_size = 512;
inline constexpr std::size_t spectrum_size = 2048;

// Streaming CSV sink. State and summation order are independent of reader blocks.
class IqDiagnostics {
  public:
    IqDiagnostics(std::uint64_t sample_rate, std::ostream* power, std::ostream* spectrum);
    void consume(std::span<const std::complex<float>> samples);
    void finish();

  private:
    void emit_power();
    std::uint64_t sample_rate_;
    std::ostream* power_;
    std::ostream* spectrum_;
    std::uint64_t sample_index_ = 0;
    double power_sum_ = 0;
    std::size_t power_count_ = 0;
    std::array<std::complex<double>, spectrum_size> spectrum_samples_{};
    std::size_t spectrum_count_ = 0;
};

// Destinations may exist. Old CSVs are preserved if processing fails.
IqSummary export_iq_diagnostics(const std::filesystem::path& input, std::uint64_t sample_rate,
                                std::ostream& summary,
                                const std::optional<std::filesystem::path>& power_path,
                                const std::optional<std::filesystem::path>& spectrum_path,
                                std::size_t block_capacity = IqFileReader::default_block_capacity);

} // namespace dab

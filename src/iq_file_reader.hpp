#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace dab {

std::complex<float> convert_iq_pair(std::uint8_t i, std::uint8_t q);

class IqFileReader {
  public:
    static constexpr std::size_t default_block_capacity = 4096;

    explicit IqFileReader(const std::filesystem::path& path,
                          std::size_t block_capacity = default_block_capacity,
                          std::size_t raw_read_bytes = 0);

    // A nonempty result has at most block_capacity complex samples. An empty
    // result means clean EOF; I/O errors and malformed final input throw.
    std::vector<std::complex<float>> read_next();

  private:
    std::size_t read_raw();
    std::vector<std::complex<float>> assemble_next_block();

    std::ifstream file_;
    std::size_t block_capacity_;
    std::vector<char> raw_buffer_;
    std::size_t raw_size_ = 0;
    std::size_t raw_offset_ = 0;
    std::optional<std::uint8_t> pending_i_;
};

} // namespace dab

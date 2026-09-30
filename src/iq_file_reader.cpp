#include "iq_file_reader.hpp"

#include <limits>
#include <stdexcept>

namespace dab {

std::complex<float> convert_iq_pair(std::uint8_t i, std::uint8_t q) {
    return {(static_cast<float>(i) - 128.0f) / 128.0f, (static_cast<float>(q) - 128.0f) / 128.0f};
}

IqFileReader::IqFileReader(const std::filesystem::path& path, std::size_t block_capacity,
                           std::size_t raw_read_bytes)
    : file_(path, std::ios::binary), block_capacity_(block_capacity) {
    if (block_capacity == 0 || block_capacity > std::numeric_limits<std::size_t>::max() / 2 ||
        block_capacity >
            static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) / 2) {
        throw std::invalid_argument("IQ block capacity must be a supported nonzero sample count");
    }
    if (raw_read_bytes == 0) {
        raw_read_bytes = block_capacity * 2;
    }
    if (raw_read_bytes > block_capacity * 2) {
        throw std::invalid_argument("Raw read size exceeds the block byte capacity");
    }
    raw_buffer_.resize(raw_read_bytes);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open IQ input file: " + path.string());
    }
}

std::size_t IqFileReader::read_raw() {
    file_.read(raw_buffer_.data(), static_cast<std::streamsize>(raw_buffer_.size()));
    const auto count = file_.gcount();
    if (file_.bad() || (file_.fail() && !file_.eof())) {
        throw std::runtime_error("Error reading IQ input file");
    }
    raw_size_ = static_cast<std::size_t>(count);
    raw_offset_ = 0;
    return raw_size_;
}

std::vector<std::complex<float>> IqFileReader::assemble_next_block() {
    std::vector<std::complex<float>> block_samples{};
    block_samples.reserve(block_capacity_);

    while (block_samples.size() < block_capacity_) {
        if (raw_offset_ == raw_size_) {
            if (read_raw() == 0) {
                if (pending_i_.has_value()) {
                    throw std::runtime_error("Unmatched I byte at end of IQ input file");
                }
                break;
            }
        }

        const auto byte = static_cast<std::uint8_t>(raw_buffer_[raw_offset_]);
        ++raw_offset_;
        if (pending_i_.has_value()) {
            block_samples.push_back(convert_iq_pair(*pending_i_, byte));
            pending_i_.reset();
        } else {
            pending_i_ = byte;
        }
    }
    return block_samples;
}

std::vector<std::complex<float>> IqFileReader::read_next() {
    return assemble_next_block();
}

} // namespace dab

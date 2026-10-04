#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include "iq_file_reader.hpp"

namespace dab {

class IqDiagnostics;

struct IqSummary {
    std::uint64_t complex_samples;
    double duration_seconds;
};

IqSummary summarize_iq_file(const std::filesystem::path& path, std::uint64_t sample_rate,
                            std::ostream& output, IqDiagnostics* diagnostics = nullptr,
                            std::size_t block_capacity = IqFileReader::default_block_capacity);

} // namespace dab

#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>

namespace dab {

struct IqSummary {
    std::uint64_t complex_samples;
    double duration_seconds;
};

IqSummary summarize_iq_file(const std::filesystem::path& path, std::uint64_t sample_rate,
                            std::ostream& output);

} // namespace dab

#pragma once

#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"
#include "null_symbol_detector.hpp"
#include "frame_periodicity_validator.hpp"

#include <cstdint>
#include <vector>

namespace dab {

struct IqSummary {
    std::uint64_t num_complex_samples = 0;
    double duration_seconds = 0;
    std::vector<NullSymbolInterval> null_symbols{};
    // One periodicity result per detected null interval, in the same order.
    std::vector<FramePeriodicityResult> period_results{};
};

// Sample rate is complex samples/s. Diagnostics are borrowed and optional.
IqSummary run_offline_iq(IqFileReader& reader, std::uint64_t sample_rate,
                         IqDiagnostics* diagnostics = nullptr);

} // namespace dab

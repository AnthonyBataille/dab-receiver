#include "offline_iq_runner.hpp"

#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"
#include "null_symbol_detector.hpp"

#include <stdexcept>

namespace dab {

IqSummary run_offline_iq(IqFileReader& reader, std::uint64_t sample_rate,
                         IqDiagnostics* diagnostics) {
    if (sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }
    NullSymbolDetectorParams params{};
    NullSymbolDetector null_detector(params);
    IqSummary summary{};

    for (;;) {
        const auto complex_samples = reader.read_next();
        if (complex_samples.empty()) {
            break;
        }
        summary.num_complex_samples += complex_samples.size();
        if (diagnostics) {
            diagnostics->consume(complex_samples);
        }
        for (auto const input_sample : complex_samples) {
            null_detector.add_power(input_sample);
            std::optional<NullSymbolInterval> interval = null_detector.update_candidate_state();
            if (interval) {
                summary.null_symbols.push_back(interval.value());
            }
            null_detector.update_reference_power();
        }
    }

    summary.duration_seconds =
        static_cast<double>(summary.num_complex_samples) / static_cast<double>(sample_rate);
    if (diagnostics) {
        diagnostics->finish();
    }
    return summary;
}

} // namespace dab

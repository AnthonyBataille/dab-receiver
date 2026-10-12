#include "offline_iq_runner.hpp"

#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"

#include <stdexcept>

namespace dab {

IqSummary run_offline_iq(IqFileReader& reader, std::uint64_t sample_rate,
                         IqDiagnostics* diagnostics) {
    if (sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }
    std::uint64_t count_complex_samples = 0;
    for (;;) {
        const auto complex_samples = reader.read_next();
        if (complex_samples.empty()) {
            break;
        }
        count_complex_samples += complex_samples.size();
        if (diagnostics) {
            diagnostics->consume(complex_samples);
        }
    }
    const double duration_seconds =
        static_cast<double>(count_complex_samples) / static_cast<double>(sample_rate);
    if (diagnostics) {
        diagnostics->finish();
    }
    return {count_complex_samples, duration_seconds};
}

} // namespace dab

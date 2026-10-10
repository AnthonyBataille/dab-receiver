#include "offline_iq_runner.hpp"

#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"

#include <stdexcept>

namespace dab {

void build_diagnostics(IqFileReader& reader, std::uint64_t sample_rate,
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
        diagnostics->consume(complex_samples);
    }
    const double duration_seconds =
        static_cast<double>(count_complex_samples) / static_cast<double>(sample_rate);
    diagnostics->set_summary_info(count_complex_samples, duration_seconds);
    diagnostics->finish();
}

} // namespace dab

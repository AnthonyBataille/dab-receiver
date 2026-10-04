#include "iq_summary.hpp"
#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"

#include <stdexcept>
#include <vector>
#include <complex>
#include <iomanip>
#include <ostream>

namespace dab {

IqSummary summarize_iq_file(const std::filesystem::path& path, std::uint64_t sample_rate,
                            std::ostream& output, IqDiagnostics* diagnostics,
                            std::size_t block_capacity) {
    if (sample_rate == 0) {
        throw std::invalid_argument("Sample rate must be positive");
    }
    IqFileReader reader(path, block_capacity);
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
    if (diagnostics) {
        diagnostics->finish();
    }
    const double duration_seconds =
        static_cast<double>(count_complex_samples) / static_cast<double>(sample_rate);

    output << "Complex samples: " << count_complex_samples << std::endl;
    output << "Duration: " << std::fixed << std::setprecision(3) << duration_seconds << " s\n";
    if (!output) {
        throw std::runtime_error("Error writing IQ summary");
    }

    IqSummary summary = {count_complex_samples, duration_seconds};
    return summary;
}

} // namespace dab

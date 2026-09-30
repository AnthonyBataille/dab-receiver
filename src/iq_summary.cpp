#include "iq_summary.hpp"
#include "iq_file_reader.hpp"

#include <stdexcept>
#include <vector>
#include <complex>
#include <iomanip>
#include <ostream>

namespace dab {

IqSummary summarize_iq_file(const std::filesystem::path& path, std::uint64_t sample_rate,
                            std::ostream& output) {
    IqFileReader reader(path);
    std::vector<std::complex<float>> complex_samples = reader.read_next();
    size_t count_complex_samples = complex_samples.size();
    while (!complex_samples.empty()) {
        complex_samples = reader.read_next();
        count_complex_samples += complex_samples.size();
    }
    const double duration_seconds =
        static_cast<double>(count_complex_samples) / static_cast<double>(sample_rate);

    output << "Complex samples: " << count_complex_samples << std::endl;
    output << "Duration: " << std::fixed << std::setprecision(3) << duration_seconds << " s\n";

    IqSummary summary = {count_complex_samples, duration_seconds};
    return summary;
}

} // namespace dab

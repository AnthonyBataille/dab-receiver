#include "iq_summary.hpp"
#include "iq_diagnostics.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " <input.iq> <sample-rate>"
              << " [--power-csv <path>] [--spectrum-csv <path>]\n"
              << "  sample-rate: positive complex samples per second (integer)\n"
              << "  --power-csv: 512-sample mean linear power versus window start time\n"
              << "  --spectrum-csv: shifted spectrum of the first 2048 complex samples\n"
              << "  CSV paths must be new files in existing directories.\n";
}

bool parse_sample_rate(std::string_view text, std::uint64_t& sample_rate) {
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(text.data(), end, sample_rate);
    return error == std::errc{} && next == end && sample_rate != 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    std::uint64_t sample_rate = 0;
    if (!parse_sample_rate(argv[2], sample_rate)) {
        std::cerr << "Error: sample rate must be a positive integer.\n";
        print_usage(argv[0]);
        return 1;
    }

    try {
        std::optional<std::filesystem::path> power_path, spectrum_path;
        for (int index = 3; index < argc; index += 2) {
            const std::string_view option = argv[index];
            auto* destination = option == "--power-csv"      ? &power_path
                                : option == "--spectrum-csv" ? &spectrum_path
                                                             : nullptr;
            if (!destination || destination->has_value() || index + 1 >= argc ||
                std::string_view(argv[index + 1]).empty() ||
                std::string_view(argv[index + 1]).starts_with("--")) {
                std::cerr << "Error: unknown, repeated, or incomplete output option: " << option
                          << '\n';
                print_usage(argv[0]);
                return 1;
            }
            *destination = argv[index + 1];
        }
        std::cout << "Input: " << argv[1] << '\n'
                  << "Format: raw unsigned 8-bit interleaved I/Q (one complex sample per pair)\n"
                  << "Sample rate: " << sample_rate << " complex samples/s\n";
        if (power_path || spectrum_path) {
            dab::export_iq_diagnostics(argv[1], sample_rate, std::cout, power_path, spectrum_path);
        } else {
            dab::summarize_iq_file(argv[1], sample_rate, std::cout);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

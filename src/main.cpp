#include "iq_file_reader.hpp"
#include "iq_diagnostics.hpp"
#include "offline_iq_runner.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " <input.iq> <sample-rate>"
              << " [--power-csv <path>] [--spectrum-csv <path>]\n"
              << "  sample-rate: positive complex samples per second (integer)\n"
              << "  --power-csv: 512-sample mean linear power versus window start time\n"
              << "  --spectrum-csv: shifted spectrum of the first 2048 complex samples\n"
              << "  CSV parent directories are created; existing CSVs are replaced on success.\n";
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
        const std::filesystem::path input_path(argv[1]);
        std::cout << "Input: " << argv[1] << '\n'
                  << "Format: raw unsigned 8-bit interleaved I/Q (one complex sample per pair)\n"
                  << "Sample rate: " << sample_rate << " complex samples/s\n";

        dab::IqFileReader reader(input_path);
        std::unique_ptr<dab::PendingCsv> power_csv, spectrum_csv;

        if (power_path) {
            power_csv = std::make_unique<dab::PendingCsv>(*power_path);
        }
        if (spectrum_path) {
            spectrum_csv = std::make_unique<dab::PendingCsv>(*spectrum_path);
        }
        for (const auto& path : {power_path, spectrum_path}) {
            if (path && std::filesystem::exists(input_path) && std::filesystem::exists(*path) &&
                std::filesystem::equivalent(input_path, *path)) {
                throw std::invalid_argument("CSV destination cannot be the IQ input file");
            }
        }
        if (power_path && spectrum_path &&
            (std::filesystem::absolute(*power_path).lexically_normal() ==
                 std::filesystem::absolute(*spectrum_path).lexically_normal() ||
             (std::filesystem::exists(*power_path) && std::filesystem::exists(*spectrum_path) &&
              std::filesystem::equivalent(*power_path, *spectrum_path)))) {
            throw std::invalid_argument("Power and spectrum CSV destinations must differ");
        }
        dab::IqDiagnostics diagnostics(sample_rate, power_csv ? &power_csv->stream() : nullptr,
                                       spectrum_csv ? &spectrum_csv->stream() : nullptr);
        diagnostics.set_summary_info(0, 0.0);
        dab::build_diagnostics(reader, sample_rate, &diagnostics);
        if (power_csv || spectrum_csv) {

            diagnostics.export_iq_diagnostics(power_csv.get(), spectrum_csv.get());
        }
        diagnostics.print_iq_summary();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

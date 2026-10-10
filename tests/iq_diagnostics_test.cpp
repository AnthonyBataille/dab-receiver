#include "iq_diagnostics.hpp"
#include "iq_file_reader.hpp"
#include "offline_iq_runner.hpp"

#define NOMINMAX
#include <Windows.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <streambuf>
#include <system_error>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// The summary API writes to cout. Restore its buffer, state and formatting even on failure.
class CoutCapture {
  public:
    explicit CoutCapture(std::ostream& output)
        : flags_(std::cout.flags()), precision_(std::cout.precision()), state_(std::cout.rdstate()),
          buffer_(std::cout.rdbuf(output.rdbuf())) {}
    ~CoutCapture() {
        std::cout.rdbuf(buffer_);
        std::cout.flags(flags_);
        std::cout.precision(precision_);
        std::cout.clear(state_);
    }
    CoutCapture(const CoutCapture&) = delete;
    CoutCapture& operator=(const CoutCapture&) = delete;

  private:
    std::ios::fmtflags flags_;
    std::streamsize precision_;
    std::ios::iostate state_;
    std::streambuf* buffer_;
};

// Assemble the public API without reproducing CLI validation or DSP calculations.
void export_diagnostics(const std::filesystem::path& input, std::uint64_t sample_rate,
                        std::ostream& summary,
                        const std::optional<std::filesystem::path>& power_path = {},
                        const std::optional<std::filesystem::path>& spectrum_path = {},
                        std::size_t capacity = dab::IqFileReader::default_block_capacity) {
    dab::IqFileReader reader(input, capacity);
    std::unique_ptr<dab::PendingCsv> power, spectrum;
    if (power_path) {
        power = std::make_unique<dab::PendingCsv>(*power_path);
    }
    if (spectrum_path) {
        spectrum = std::make_unique<dab::PendingCsv>(*spectrum_path);
    }
    dab::IqDiagnostics diagnostics(sample_rate, power ? &power->stream() : nullptr,
                                   spectrum ? &spectrum->stream() : nullptr);
    dab::build_diagnostics(reader, sample_rate, &diagnostics);
    diagnostics.export_iq_diagnostics(power.get(), spectrum.get());
    CoutCapture capture(summary);
    diagnostics.print_iq_summary();
}

struct Fixture {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("dab_diagnostics_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path input = directory / "input.iq";
    std::filesystem::path power = directory / "power.csv";
    std::filesystem::path spectrum = directory / "spectrum.csv";

    explicit Fixture(const std::vector<unsigned char>& bytes = {}) {
        REQUIRE(std::filesystem::create_directory(directory));
        std::ofstream file(input, std::ios::binary);
        for (const auto byte : bytes) {
            file.put(static_cast<char>(byte));
        }
        REQUIRE(file.good());
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    void check_clean() const {
        CHECK_FALSE(std::filesystem::exists(power));
        CHECK_FALSE(std::filesystem::exists(spectrum));
        CHECK(std::distance(std::filesystem::directory_iterator(directory),
                            std::filesystem::directory_iterator{}) == 1);
    }
};

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::vector<double>> rows(const std::string& csv) {
    std::istringstream input(csv);
    std::string line;
    std::getline(input, line);
    std::vector<std::vector<double>> result;
    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream values(line);
        std::vector<double> row;
        double value;
        while (values >> value) {
            row.push_back(value);
        }
        result.push_back(row);
    }
    return result;
}

// Deterministically simulate a device/disk stream that stops accepting bytes.
class LimitedBuffer : public std::streambuf {
  public:
    explicit LimitedBuffer(std::size_t limit) : remaining_(limit) {}

  private:
    int_type overflow(int_type value) override {
        if (remaining_ == 0) {
            return traits_type::eof();
        }
        --remaining_;
        return value;
    }
    std::size_t remaining_;
};

} // namespace

TEST_CASE("power windows and sample-index time are independent of reader capacity",
          "[diagnostics]") {
    std::vector<unsigned char> bytes;
    // First window: 256 powers of 1 and 256 powers of 0.25 => 0.625.
    // Final window: three powers of 2 => 2.
    for (int n = 0; n < 515; ++n) {
        bytes.push_back(n < 256 || n >= 512 ? 0 : 192);
        bytes.push_back(n >= 512 ? 0 : 128);
    }
    const Fixture fixture(bytes);
    std::string reference;
    for (const std::size_t capacity : {1, 7, 511, 512, 513, 4096}) {
        std::ostringstream power, summary;
        dab::IqDiagnostics diagnostics(1000, &power, nullptr);
        dab::IqFileReader reader(fixture.input, capacity);
        dab::build_diagnostics(reader, 1000, &diagnostics);
        {
            CoutCapture capture(summary);
            diagnostics.print_iq_summary();
        }
        CHECK(summary.str() == "Complex samples: 515\nDuration: 0.515 s\n");
        CHECK(reader.read_next().empty());
        const auto data = rows(power.str());
        REQUIRE(data.size() == 2);
        CHECK(data[0] == std::vector<double>{0, 0.625, 512});
        CHECK(data[1] == std::vector<double>{0.512, 2, 3});
        if (reference.empty()) {
            reference = power.str();
        }
        CHECK(power.str() == reference);
    }
}

TEST_CASE("empty recording has a zero summary with or without power export", "[diagnostics]") {
    const Fixture fixture;
    std::ostringstream summary;
    std::optional<std::filesystem::path> power;
    SECTION("summary only") {}
    SECTION("power export") {
        power = fixture.power;
    }
    export_diagnostics(fixture.input, 123, summary, power);
    CHECK(summary.str() == "Complex samples: 0\nDuration: 0.000 s\n");
    if (power) {
        CHECK(read_text(*power) == "window_start_seconds,mean_linear_power,sample_count\n");
    }
}

TEST_CASE("export creates missing parent directories", "[diagnostics]") {
    const Fixture fixture(std::vector<unsigned char>(4096, 128));
    const auto power = fixture.directory / "new" / "nested" / "power.csv";
    const auto spectrum = fixture.directory / "new" / "nested" / "spectrum.csv";
    std::ostringstream summary;
    export_diagnostics(fixture.input, 2048, summary, power, spectrum);
    CHECK(summary.str() == "Complex samples: 2048\nDuration: 1.000 s\n");
    CHECK(rows(read_text(power)).size() == 4);
    CHECK(rows(read_text(spectrum)).size() == 2048);
}

TEST_CASE("a second export replaces both CSVs or preserves them on failure",
          "[diagnostics][error]") {
    const Fixture fixture(std::vector<unsigned char>(4096, 128));
    std::ostringstream summary;
    export_diagnostics(fixture.input, 2048, summary, fixture.power, fixture.spectrum);
    const auto old_power = read_text(fixture.power);
    const auto old_spectrum = read_text(fixture.spectrum);
    SECTION("successful replacement") {
        // All-zero bytes convert to -1-j, unlike the first recording's zero signal.
        std::ofstream replacement(fixture.input, std::ios::binary | std::ios::trunc);
        for (int byte = 0; byte < 4096; ++byte) {
            replacement.put(0);
        }
        replacement.close();
        REQUIRE(replacement.good());
        export_diagnostics(fixture.input, 2048, summary, fixture.power, fixture.spectrum);
        CHECK(read_text(fixture.power) != old_power);
        CHECK(read_text(fixture.spectrum) != old_spectrum);
        CHECK(rows(read_text(fixture.power))[0][1] == 2);
    }
    SECTION("failed replacement") {
        SECTION("processing fails before publication") {
            std::filesystem::resize_file(fixture.input, 4097);
            CHECK_THROWS_WITH(
                export_diagnostics(fixture.input, 2048, summary, fixture.power, fixture.spectrum),
                "Unmatched I byte at end of IQ input file");
        }
        SECTION("second publication fails after the first") {
            // Deny delete sharing so Windows cannot move the second destination aside.
            {
                std::ofstream changed(fixture.input, std::ios::binary | std::ios::trunc);
                const std::vector<char> bytes(4096, 0);
                changed.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                REQUIRE(changed.good());
            }
            const HANDLE locked =
                CreateFileW(fixture.spectrum.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            REQUIRE(locked != INVALID_HANDLE_VALUE);
            CHECK_THROWS(
                export_diagnostics(fixture.input, 2048, summary, fixture.power, fixture.spectrum));
            CloseHandle(locked);
        }
        CHECK(read_text(fixture.power) == old_power);
        CHECK(read_text(fixture.spectrum) == old_spectrum);
    }
    CHECK(std::distance(std::filesystem::directory_iterator(fixture.directory),
                        std::filesystem::directory_iterator{}) == 3);
}

TEST_CASE("rectangular DFT has the correct sign shift and normalized tone power", "[diagnostics]") {
    for (const int sign : {1, -1, 0}) {
        // Exact four-sample complex sinusoid: +/- Fs/4, amplitude 0.5.
        const std::array<std::complex<float>, 4> cycle = {
            std::complex<float>{0.5F, 0}, {0, 0.5F}, {-0.5F, 0}, {0, -0.5F}};
        std::vector<std::complex<float>> samples(2048);
        for (std::size_t n = 0; n < samples.size(); ++n) {
            samples[n] = sign == 0 ? cycle[0] : cycle[n % 4];
            if (sign < 0) {
                samples[n] = std::conj(samples[n]);
            }
        }
        std::ostringstream spectrum;
        dab::IqDiagnostics diagnostics(2'048'000, nullptr, &spectrum);
        diagnostics.consume(std::span(samples).first(13));
        diagnostics.consume(std::span(samples).subspan(13));
        // Later samples must not alter the initial spectrum.
        diagnostics.consume(std::vector<std::complex<float>>(17, {1, 1}));
        diagnostics.finish();
        const auto data = rows(spectrum.str());
        REQUIRE(data.size() == 2048);
        const auto peak = static_cast<std::size_t>(1024 + sign * 512);
        for (std::size_t bin = 0; bin < data.size(); ++bin) {
            REQUIRE(data[bin].size() == 2);
            CHECK(data[bin][0] == (static_cast<double>(bin) - 1024) * 1000);
            if (bin == peak) {
                CHECK(data[bin][1] == Catch::Approx(0.25).margin(1e-12));
            } else {
                CHECK(data[bin][1] < 1e-20);
            }
        }
    }
}

TEST_CASE("file exports share one complete summary and have bounded initial spectrum",
          "[diagnostics]") {
    std::vector<unsigned char> bytes(4098, 128);
    for (std::size_t n = 0; n < 4096; n += 2) {
        bytes[n] = 192;
    }
    const Fixture fixture(bytes);
    std::ostringstream summary;
    export_diagnostics(fixture.input, 2048, summary, fixture.power, fixture.spectrum, 13);
    CHECK(summary.str() == "Complex samples: 2049\nDuration: 1.000 s\n");
    const auto power = rows(read_text(fixture.power));
    REQUIRE(power.size() == 5);
    CHECK(power.back() == std::vector<double>{1, 0, 1});
    const auto spectrum = rows(read_text(fixture.spectrum));
    REQUIRE(spectrum.size() == 2048);
    CHECK(spectrum[1024][1] == Catch::Approx(0.25));
}

TEST_CASE("failed processing publishes no CSV and cleans staging", "[diagnostics][error]") {
    const Fixture fixture(std::vector<unsigned char>(1024, 128));
    std::ostringstream summary;
    SECTION("short spectrum") {
        CHECK_THROWS_WITH(
            export_diagnostics(fixture.input, 1000, summary, fixture.power, fixture.spectrum),
            "Spectrum requires at least 2048 complex samples; received 512");
    }
    SECTION("missing input") {
        const auto missing = fixture.directory / "missing.iq";
        CHECK_THROWS_WITH(export_diagnostics(missing, 1000, summary, fixture.power),
                          "Cannot open IQ input file: " + missing.string());
    }
    SECTION("lone final I byte") {
        std::filesystem::resize_file(fixture.input, 1);
        CHECK_THROWS_WITH(export_diagnostics(fixture.input, 1000, summary),
                          "Unmatched I byte at end of IQ input file");
    }
    SECTION("malformed input after complete power windows") {
        std::filesystem::resize_file(fixture.input, 8193);
        CHECK_THROWS_WITH(
            export_diagnostics(fixture.input, 1000, summary, fixture.power, fixture.spectrum),
            "Unmatched I byte at end of IQ input file");
    }
    SECTION("invalid sample rate") {
        CHECK_THROWS(export_diagnostics(fixture.input, 0, summary, fixture.power, {}));
    }
    SECTION("second output path is a directory") {
        CHECK_THROWS(
            export_diagnostics(fixture.input, 1000, summary, fixture.power, fixture.directory));
    }
    fixture.check_clean();
}

TEST_CASE("directory destinations are rejected without modifying input", "[diagnostics][error]") {
    const Fixture fixture({0, 128});
    const auto before = read_text(fixture.input);
    CHECK_THROWS_AS(dab::PendingCsv(fixture.directory), std::runtime_error);
    CHECK(read_text(fixture.input) == before);
    fixture.check_clean();
}

TEST_CASE("spectrum can be exported without power", "[diagnostics]") {
    const Fixture fixture(std::vector<unsigned char>(4096, 192));
    std::ostringstream summary;
    export_diagnostics(fixture.input, 2048, summary, {}, fixture.spectrum);
    CHECK_FALSE(std::filesystem::exists(fixture.power));
    const auto data = rows(read_text(fixture.spectrum));
    REQUIRE(data.size() == 2048);
    CHECK(data[1024][1] == Catch::Approx(0.5));
    CHECK(summary.str() == "Complex samples: 2048\nDuration: 1.000 s\n");
}

TEST_CASE("CSV stream write failures are reported", "[diagnostics][error]") {
    LimitedBuffer buffer(100);
    std::ostream failing(&buffer);
    SECTION("power data write") {
        dab::IqDiagnostics diagnostics(1000, &failing, nullptr);
        CHECK_THROWS_AS(diagnostics.consume(std::vector<std::complex<float>>(4096, {1, 0})),
                        std::runtime_error);
    }
    SECTION("spectrum data write") {
        dab::IqDiagnostics diagnostics(1000, nullptr, &failing);
        diagnostics.consume(std::vector<std::complex<float>>(2048, {0, 0}));
        CHECK_THROWS_AS(diagnostics.finish(), std::runtime_error);
    }
    SECTION("header write") {
        failing.setstate(std::ios::badbit);
        CHECK_THROWS_AS(dab::IqDiagnostics(1000, &failing, nullptr), std::runtime_error);
    }
}

TEST_CASE("IQ summary counts short recordings and formats their durations", "[iq][exercise]") {
    SECTION("three samples at two samples per second") {
        const Fixture input({0, 128, 255, 0, 128, 255});
        std::ostringstream output;
        export_diagnostics(input.input, 2, output);

        CHECK(output.str() == "Complex samples: 3\nDuration: 1.500 s\n");
    }
    SECTION("one sample at four samples per second") {
        const Fixture input({0, 128});
        std::ostringstream output;
        export_diagnostics(input.input, 4, output);

        CHECK(output.str() == "Complex samples: 1\nDuration: 0.250 s\n");
    }
}

TEST_CASE("IQ summary counts complex samples and computes duration of a reference-sized file",
          "[iq]") {
    constexpr std::uint64_t file_bytes = 40'960'000;
    const Fixture input({});
    std::filesystem::resize_file(input.input, file_bytes);
    std::ostringstream output;
    export_diagnostics(input.input, 2'048'000, output);

    CHECK(output.str() == "Complex samples: 20480000\nDuration: 10.000 s\n");
}

TEST_CASE("offline runner rejects zero sample rate", "[iq][error]") {
    const Fixture input({0, 128});
    dab::IqFileReader reader(input.input);
    dab::IqDiagnostics diagnostics(1000, nullptr, nullptr);
    CHECK_THROWS_AS(dab::build_diagnostics(reader, 0, &diagnostics), std::invalid_argument);
    CHECK(reader.read_next().size() == 1);
}

TEST_CASE("summary output failures are reported", "[iq][error]") {
    std::ostringstream output;
    CoutCapture capture(output);
    dab::IqDiagnostics diagnostics(1000, nullptr, nullptr);
    diagnostics.set_summary_info(3, 0.003);
    std::cout.setstate(std::ios::badbit);
    CHECK_THROWS_AS(diagnostics.print_iq_summary(), std::runtime_error);
}

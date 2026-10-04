#include "iq_diagnostics.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
        const auto result =
            dab::summarize_iq_file(fixture.input, 1000, summary, &diagnostics, capacity);
        CHECK(result.complex_samples == 515);
        CHECK(result.duration_seconds == 0.515);
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

TEST_CASE("empty recording exports only a power header", "[diagnostics]") {
    const Fixture fixture;
    std::ostringstream summary;
    const auto result = dab::export_iq_diagnostics(fixture.input, 123, summary, fixture.power, {});
    CHECK(result.complex_samples == 0);
    CHECK(result.duration_seconds == 0);
    CHECK(read_text(fixture.power) == "window_start_seconds,mean_linear_power,sample_count\n");
}

TEST_CASE("rectangular DFT has the correct sign shift and normalized tone power", "[diagnostics]") {
    for (const int sign : {1, -1, 0}) {
        // Exact four-sample complex sinusoid: +/- Fs/4, amplitude 0.5.
        const std::array<std::complex<float>, 4> cycle = {
            std::complex<float>{0.5F, 0}, {0, 0.5F}, {-0.5F, 0}, {0, -0.5F}};
        std::vector<std::complex<float>> samples(dab::spectrum_size);
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
    const auto result = dab::export_iq_diagnostics(fixture.input, 2048, summary, fixture.power,
                                                   fixture.spectrum, 13);
    CHECK(result.complex_samples == 2049);
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
        CHECK_THROWS_WITH(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.power,
                                                     fixture.spectrum),
                          "Spectrum requires at least 2048 complex samples; received 512");
    }
    SECTION("missing input") {
        CHECK_THROWS(dab::export_iq_diagnostics(fixture.directory / "missing.iq", 1000, summary,
                                                fixture.power, {}));
    }
    SECTION("malformed input after complete power windows") {
        std::filesystem::resize_file(fixture.input, 8193);
        CHECK_THROWS_WITH(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.power,
                                                     fixture.spectrum),
                          "Unmatched I byte at end of IQ input file");
    }
    SECTION("invalid sample rate") {
        CHECK_THROWS(dab::export_iq_diagnostics(fixture.input, 0, summary, fixture.power, {}));
    }
    SECTION("cannot open second output") {
        CHECK_THROWS(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.power,
                                                fixture.directory / "missing" / "spectrum.csv"));
    }
    SECTION("publication collision rolls back first published CSV") {
        std::filesystem::resize_file(fixture.input, 4096);
        CHECK_THROWS(
            dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.power, fixture.power));
    }
    SECTION("summary output fails before publication") {
        summary.setstate(std::ios::badbit);
        CHECK_THROWS(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.power, {}));
    }
    fixture.check_clean();
}

TEST_CASE("existing destinations including the IQ input are preserved", "[diagnostics][error]") {
    const Fixture fixture({0, 128});
    const auto before = read_text(fixture.input);
    std::ostringstream summary;
    CHECK_THROWS(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.input, {}));
    CHECK(read_text(fixture.input) == before);
    CHECK_THROWS(dab::export_iq_diagnostics(fixture.input, 1000, summary, fixture.directory, {}));
    fixture.check_clean();
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

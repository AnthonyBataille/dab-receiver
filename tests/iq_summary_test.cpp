#include "iq_summary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

class InputFixture {
  public:
    explicit InputFixture(const std::vector<std::uint8_t>& bytes)
        : path_(std::filesystem::temp_directory_path() /
                ("dab_summary_test_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 ".iq")) {
        std::ofstream output(path_, std::ios::binary);
        REQUIRE(output.is_open());
        for (const auto byte : bytes) {
            output.put(static_cast<char>(byte));
        }
        REQUIRE(output.good());
    }

    ~InputFixture() {
        std::filesystem::remove(path_);
    }

    const std::filesystem::path& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("IQ summary counts short recordings and formats their durations", "[iq][exercise]") {
    SECTION("three samples at two samples per second") {
        const InputFixture input({0, 128, 255, 0, 128, 255});
        std::ostringstream output;
        const auto summary = dab::summarize_iq_file(input.path(), 2, output);
        CHECK(summary.complex_samples == 3);
        CHECK(summary.duration_seconds == 1.5);
        CHECK(output.str().find("Complex samples: 3") != std::string::npos);
        CHECK(output.str().find("Duration: 1.500") != std::string::npos);
    }
    SECTION("one sample at four samples per second") {
        const InputFixture input({0, 128});
        std::ostringstream output;
        const auto summary = dab::summarize_iq_file(input.path(), 4, output);
        CHECK(summary.complex_samples == 1);
        CHECK(summary.duration_seconds == 0.25);
        CHECK(output.str().find("Complex samples: 1") != std::string::npos);
        CHECK(output.str().find("Duration: 0.250") != std::string::npos);
    }
}

TEST_CASE("IQ summary counts complex samples and computes duration of a reference-sized file",
          "[iq]") {
    constexpr std::uint64_t file_bytes = 40'960'000;
    constexpr std::uint64_t count_complex_samples = file_bytes / 2;
    const InputFixture input({});
    std::filesystem::resize_file(input.path(), file_bytes);
    std::ostringstream output;
    const auto summary = dab::summarize_iq_file(input.path(), 2'048'000, output);
    CHECK(summary.complex_samples == count_complex_samples);
    CHECK(summary.duration_seconds == 10.0);
    CHECK(output.str().find("Complex samples: 20480000") != std::string::npos);
    CHECK(output.str().find("Duration: 10.000") != std::string::npos);
}

TEST_CASE("IQ summary reports a missing input file", "[iq][error]") {
    const InputFixture input({});
    auto missing = input.path();
    missing += ".missing";
    std::ostringstream output;

    CHECK_THROWS_WITH(dab::summarize_iq_file(missing, 2'048'000, output),
                      "Cannot open IQ input file: " + missing.string());
}

TEST_CASE("IQ summary rejects a lone final I byte", "[iq][error]") {
    const InputFixture input({128});
    std::ostringstream output;

    CHECK_THROWS_WITH(dab::summarize_iq_file(input.path(), 2'048'000, output),
                      "Unmatched I byte at end of IQ input file");
}

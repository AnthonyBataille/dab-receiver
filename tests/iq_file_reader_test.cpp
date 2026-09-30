#include "iq_file_reader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

class InputFixture {
  public:
    explicit InputFixture(const std::vector<std::uint8_t>& bytes)
        : path_(std::filesystem::temp_directory_path() /
                ("dab_iq_test_" +
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

TEST_CASE("byte pair conversion uses I then Q and specified normalization", "[iq][exercise]") {
    CHECK(dab::convert_iq_pair(0, 128) == std::complex<float>(-1.0F, 0.0F));
    CHECK(dab::convert_iq_pair(255, 0) == std::complex<float>(127.0F / 128.0F, -1.0F));
    CHECK(dab::convert_iq_pair(128, 255) == std::complex<float>(0.0F, 127.0F / 128.0F));
}

TEST_CASE("reader returns bounded blocks in I/Q order", "[iq][exercise]") {
    const InputFixture input({0, 128, 255, 0, 128, 255, 128, 128, 0, 0});
    dab::IqFileReader reader(input.path(), 2);
    CHECK(reader.read_next() ==
          std::vector<std::complex<float>>{{-1.0F, 0.0F}, {127.0F / 128.0F, -1.0F}});
    CHECK(reader.read_next() ==
          std::vector<std::complex<float>>{{0.0F, 127.0F / 128.0F}, {0.0F, 0.0F}});
    CHECK(reader.read_next() == std::vector<std::complex<float>>{{-1.0F, -1.0F}});
    CHECK(reader.read_next().empty());
}

TEST_CASE("a raw read boundary may split an I/Q pair", "[iq][exercise]") {
    const InputFixture input({0, 128, 255, 0, 128, 255});
    dab::IqFileReader reader(input.path(), 2, 3);
    CHECK(reader.read_next() ==
          std::vector<std::complex<float>>{{-1.0F, 0.0F}, {127.0F / 128.0F, -1.0F}});
    CHECK(reader.read_next() == std::vector<std::complex<float>>{{0.0F, 127.0F / 128.0F}});
    CHECK(reader.read_next().empty());
}

TEST_CASE("empty input returns clean EOF", "[iq][exercise]") {
    const InputFixture input({});
    dab::IqFileReader reader(input.path());
    CHECK(reader.read_next().empty());
    CHECK(reader.read_next().empty());
}

TEST_CASE("a lone final byte is rejected", "[iq][exercise]") {
    const InputFixture input({128});
    dab::IqFileReader reader(input.path(), 2, 1);
    CHECK_THROWS_AS(reader.read_next(), std::runtime_error);
}

TEST_CASE("reader rejects missing files and invalid capacities", "[iq]") {
    const InputFixture input({});
    CHECK_THROWS_AS(dab::IqFileReader(input.path(), 0), std::invalid_argument);
    CHECK_THROWS_AS(dab::IqFileReader(input.path(), 1, 3), std::invalid_argument);
    auto missing = input.path();
    missing += ".missing";
    CHECK_THROWS_AS(dab::IqFileReader(missing), std::runtime_error);
}

TEST_CASE("reader streams a reference-sized file instead of snapshotting it", "[iq]") {
    constexpr std::uintmax_t file_bytes = 40'960'000;
    constexpr std::size_t block_capacity = 4096;
    const InputFixture input({});
    std::filesystem::resize_file(input.path(), file_bytes);

    dab::IqFileReader reader(input.path(), block_capacity);
    const auto first = reader.read_next();
    REQUIRE(first.size() == block_capacity);

    // Change bytes far beyond the first block after reading has begun. An
    // eager snapshot would keep the old final sample.
    {
        std::fstream output(input.path(), std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE(output.is_open());
        output.seekp(static_cast<std::streamoff>(file_bytes - 2));
        output.put(static_cast<char>(128));
        output.put(static_cast<char>(255));
        REQUIRE(output.good());
    }

    std::size_t total_samples = first.size();
    std::complex<float> last_sample = first.back();
    for (;;) {
        const auto block = reader.read_next();
        if (block.empty()) {
            break;
        }
        total_samples += block.size();
        last_sample = block.back();
    }
    CHECK(total_samples == file_bytes / 2);
    CHECK(last_sample == std::complex<float>(0.0F, 127.0F / 128.0F));
}

TEST_CASE("every returned block respects the configured sample capacity", "[iq]") {
    const InputFixture input({0, 128, 255, 0, 128, 255, 128, 128, 0, 0, 255, 255, 0, 255, 128, 0});

    for (const std::size_t capacity : {std::size_t{1}, std::size_t{3}, std::size_t{5}}) {
        dab::IqFileReader reader(input.path(), capacity, capacity * 2 - 1);
        std::size_t total_samples = 0;
        for (;;) {
            const auto block = reader.read_next();
            if (block.empty()) {
                break;
            }
            CHECK(block.size() <= capacity);
            total_samples += block.size();
        }
        CHECK(total_samples == 8);
    }
}

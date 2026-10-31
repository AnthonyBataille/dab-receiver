#include "null_symbol_detector.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(dab::NullSymbolInterval::start), std::uint64_t>);
static_assert(std::is_same_v<decltype(dab::NullSymbolInterval::end), std::uint64_t>);

namespace {
struct DetectorFixture {
    dab::NullSymbolDetector detector;
    std::vector<dab::RejectedCandidateInfo> rejected;
    std::vector<dab::NullSymbolInterval> accepted;

    explicit DetectorFixture(dab::NullSymbolDetectorParams params = {}) : detector(params) {}

    void feed(std::complex<float> sample, std::size_t count = 1) {
        for (std::size_t n = 0; n < count; ++n) {
            std::optional<dab::RejectedCandidateInfo> rejection;
            detector.add_power(sample);
            if (const auto event = detector.update_candidate_state(&rejection)) {
                accepted.push_back(*event);
            }
            if (rejection) {
                rejected.push_back(*rejection);
            }
            detector.update_reference_power();
        }
    }

    void feed_chunks(std::span<const std::complex<float>> samples, std::size_t capacity) {
        for (std::size_t begin = 0; begin < samples.size(); begin += capacity) {
            const auto block = samples.subspan(begin, std::min(capacity, samples.size() - begin));
            for (std::size_t n = 0; n < block.size(); ++n) {
                const auto previous_count = accepted.size();
                feed(block[n]);
                if (accepted.size() != previous_count) {
                    CHECK(accepted.back().end == begin + n);
                }
            }
        }
    }
};
} // namespace

TEST_CASE("rejection output is cleared and describes only the current call",
          "[null-symbol-detector]") {
    // Direct calls intentionally bypass the fixture to test the output-pointer contract.
    dab::NullSymbolDetector detector({1, 1.0, 0.1, 0.2, 2, 3});
    std::optional<dab::RejectedCandidateInfo> rejection =
        dab::RejectedCandidateInfo{99, 100, dab::CandidateRejectionReason::TOO_LONG};
    const auto feed = [&](float amplitude) {
        detector.add_power({amplitude, 0.0F});
        const auto event = detector.update_candidate_state(&rejection);
        detector.update_reference_power();
        return event;
    };
    SECTION("before input and during warmup") {
        CHECK_FALSE(detector.update_candidate_state(&rejection));
        CHECK_FALSE(rejection);
        rejection = dab::RejectedCandidateInfo{99, 100, dab::CandidateRejectionReason::TOO_SHORT};
        dab::NullSymbolDetector warming({4, 1.0, 0.1, 0.2, 2, 3});
        warming.add_power({1.0F, 0.0F});
        CHECK_FALSE(warming.update_candidate_state(&rejection));
        CHECK_FALSE(rejection);
    }
    SECTION("short rejection, new candidate, and acceptance") {
        REQUIRE_FALSE(feed(1.0F));
        REQUIRE_FALSE(feed(0.0F));
        REQUIRE_FALSE(feed(1.0F));
        REQUIRE(rejection);
        CHECK(rejection->start == 1);
        CHECK(rejection->end == 2);
        CHECK(rejection->reject_reason == dab::CandidateRejectionReason::TOO_SHORT);
        REQUIRE_FALSE(feed(0.0F));
        CHECK_FALSE(rejection);
        REQUIRE_FALSE(feed(0.0F));
        rejection = dab::RejectedCandidateInfo{99, 100, dab::CandidateRejectionReason::TOO_LONG};
        const auto accepted = feed(1.0F);
        REQUIRE(accepted);
        CHECK(accepted->start == 3);
        CHECK(accepted->end == 5);
        CHECK_FALSE(rejection);
    }
    SECTION("long rejection and recovery") {
        REQUIRE_FALSE(feed(1.0F));
        for (int n = 0; n < 4; ++n) {
            REQUIRE_FALSE(feed(0.0F));
        }
        REQUIRE(rejection);
        CHECK(rejection->start == 1);
        CHECK(rejection->end == 4);
        CHECK(rejection->reject_reason == dab::CandidateRejectionReason::TOO_LONG);
        REQUIRE_FALSE(feed(0.0F));
        CHECK_FALSE(rejection);
        REQUIRE_FALSE(feed(0.1F));
        CHECK_FALSE(rejection);
    }
}

TEST_CASE("omitted or null rejection output does not change detection or recovery",
          "[null-symbol-detector]") {
    const bool explicit_null = GENERATE(false, true);
    dab::NullSymbolDetector detector({1, 1.0, 0.1, 0.2, 2, 3});
    // Short rejection, long rejection, weaker-signal recovery, then acceptance.
    const float amplitudes[] = {1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.1F, 0.0F, 0.0F, 0.1F};
    std::vector<dab::NullSymbolInterval> accepted;
    for (const float amplitude : amplitudes) {
        detector.add_power({amplitude, 0.0F});
        const auto interval = explicit_null ? detector.update_candidate_state(nullptr)
                                            : detector.update_candidate_state();
        if (interval) {
            accepted.push_back(*interval);
        }
        detector.update_reference_power();
    }
    REQUIRE(accepted.size() == 1);
    CHECK(accepted[0].start == 8);
    CHECK(accepted[0].end == 10);
}

TEST_CASE("startup acquisition ignores partial windows and requires confirmation",
          "[null-symbol-detector]") {
    // A slow EMA checks that acquisition seeds a reference rather than ramping from zero.
    DetectorFixture fixture({4, 0.0001, 0.1, 0.2, 1, 4});
    fixture.feed({1.0F, 0.0F});
    fixture.feed({}, 12); // A partial positive window must not arm detection.
    CHECK(fixture.accepted.empty());
    // Positive full windows at 13..16 complete acquisition before the next dip.
    fixture.feed({1.0F, 0.0F}, 7);
    fixture.feed({}, 4);
    CHECK(fixture.accepted.empty());
    fixture.feed({1.0F, 0.0F});
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 23);
    CHECK(fixture.accepted[0].end == 24);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("startup zero input skips the leading null across chunks", "[null-symbol-detector]") {
    const auto capacity = GENERATE(1, 7, 127, 4096);
    std::vector<std::complex<float>> samples(1500, std::complex<float>{});
    samples.resize(6500, {0.5F, 0.5F});
    samples.resize(9156, {});
    samples.resize(9300, {0.5F, 0.5F});
    DetectorFixture fixture;
    fixture.feed_chunks(samples, capacity);
    // The 128-sample boxcar crosses entry after 103 zeros. Reference adaptation
    // before entry means 51 returning samples suffice to cross the frozen exit ratio.
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 6602);
    CHECK(fixture.accepted[0].end == 9206);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("startup with a nonzero null floor does not infer a leading crossing",
          "[null-symbol-detector]") {
    DetectorFixture fixture({4, 0.01, 0.1, 0.2, 8, 12});
    fixture.feed({0.01F, 0.0F}, 100);
    fixture.feed({1.0F, 0.0F}, 1000);
    fixture.feed({0.01F, 0.0F}, 12);
    CHECK(fixture.accepted.empty());
    fixture.feed({1.0F, 0.0F});
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 1103);
    CHECK(fixture.accepted[0].end == 1112);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("zero and short startup input emit no intervals", "[null-symbol-detector]") {
    DetectorFixture fixture;
    SECTION("zero input") {
        fixture.feed({}, 20000);
    }
    SECTION("input shorter than smoothing window") {
        fixture.feed({1.0F, 0.0F}, 127);
    }
    CHECK(fixture.accepted.empty());
    CHECK(fixture.rejected.empty());
}

TEST_CASE("null detector validates configuration at construction", "[null-symbol-detector]") {
    CHECK_NOTHROW(dab::NullSymbolDetector{});
    dab::NullSymbolDetectorParams params;
    SECTION("valid duration bounds") {
        params.min_candidate_duration = 3;
        params.max_candidate_duration =
            GENERATE(std::uint64_t{3}, std::uint64_t{4}, std::numeric_limits<std::uint64_t>::max());
        CHECK_NOTHROW(dab::NullSymbolDetector{params});
        return;
    }
    SECTION("zero window") {
        params.window_length = 0;
    }
    SECTION("zero minimum duration") {
        params.min_candidate_duration = 0;
    }
    SECTION("reversed duration bounds") {
        params.min_candidate_duration = 3000;
    }
    SECTION("zero entry") {
        params.threshold_in = 0.0;
    }
    SECTION("negative entry") {
        params.threshold_in = -0.1;
    }
    SECTION("equal thresholds") {
        params.threshold_in = params.threshold_out;
    }
    SECTION("reversed thresholds") {
        params.threshold_in = 0.5;
    }
    SECTION("exit at unity") {
        params.threshold_out = 1.0;
    }
    SECTION("exit above unity") {
        params.threshold_out = 1.1;
    }
    SECTION("nonfinite settings") {
        const auto value = GENERATE(std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity());
        SECTION("rate") {
            params.reference_power_update_rate = value;
        }
        SECTION("entry") {
            params.threshold_in = value;
        }
        SECTION("exit") {
            params.threshold_out = value;
        }
    }
    SECTION("invalid finite rate") {
        params.reference_power_update_rate = GENERATE(-0.1, 0.0, 1.1);
    }
    CHECK_THROWS_AS(dab::NullSymbolDetector{params}, std::invalid_argument);
}

TEST_CASE("inclusive duration bounds accept endpoints and reject adjacent durations",
          "[null-symbol-detector]") {
    const auto duration = GENERATE(2389, 2390, 2656, 2922, 2923);
    DetectorFixture fixture({1, 1.0, 0.1, 0.2});
    fixture.feed({1.0F, 0.0F});
    fixture.feed({}, duration);
    CHECK(fixture.accepted.empty());
    fixture.feed({1.0F, 0.0F});
    if (duration >= 2390 && duration <= 2922) {
        REQUIRE(fixture.accepted.size() == 1);
        CHECK(fixture.accepted[0].start == 1);
        CHECK(fixture.accepted[0].end == static_cast<std::uint64_t>(duration + 1));
        CHECK(fixture.rejected.empty());
    } else {
        CHECK(fixture.accepted.empty());
        REQUIRE(fixture.rejected.size() == 1);
        CHECK(fixture.rejected[0].start == 1);
        CHECK(fixture.rejected[0].end == (duration < 2390 ? 2390 : 2923));
        CHECK(fixture.rejected[0].reject_reason == (duration < 2390
                                                        ? dab::CandidateRejectionReason::TOO_SHORT
                                                        : dab::CandidateRejectionReason::TOO_LONG));
    }
}

TEST_CASE("timeout rejects on the first sample past maximum", "[null-symbol-detector]") {
    DetectorFixture fixture({1, 1.0, 0.1, 0.2, 2, 3});
    fixture.feed({1.0F, 0.0F});
    fixture.feed({}, 3);
    CHECK(fixture.rejected.empty());
    fixture.feed({});
    REQUIRE(fixture.rejected.size() == 1);
    CHECK(fixture.rejected[0].start == 1);
    // Timeout diagnostics use the current sample index, not a later exit.
    CHECK(fixture.rejected[0].end == 4);
    CHECK(fixture.rejected[0].reject_reason == dab::CandidateRejectionReason::TOO_LONG);
    CHECK(fixture.accepted.empty());
}

TEST_CASE("silence recovery adapts to weaker signal across input chunks",
          "[null-symbol-detector]") {
    const auto capacity = GENERATE(1, 7, 128, 4096);
    std::vector<std::complex<float>> samples(2400, {1.0F, 0.0F});
    samples.resize(3400, {});
    samples.resize(4400, {0.1F, 0.0F});
    samples.resize(4412, {});
    samples.resize(4500, {0.1F, 0.0F});
    DetectorFixture fixture({4, 0.01, 0.1, 0.2, 8, 12});
    fixture.feed_chunks(samples, capacity);
    // Four-sample smoothing shifts entry by 3 samples and exit by 0: duration 9.
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 4403);
    CHECK(fixture.accepted[0].end == 4412);
    REQUIRE(fixture.rejected.size() == 1);
    CHECK(fixture.rejected[0].start == 2403);
    CHECK(fixture.rejected[0].end == 2415);
    CHECK(fixture.rejected[0].reject_reason == dab::CandidateRejectionReason::TOO_LONG);
}

TEST_CASE("null detector uses its copied configuration and custom duration limits",
          "[null-symbol-detector]") {
    dab::NullSymbolDetectorParams params{1, 1.0, 0.1, 0.2, 2, 4};
    DetectorFixture fixture(params);
    params.window_length = 0;
    params.threshold_in = 0.0;
    params.min_candidate_duration = 100;
    CHECK(fixture.detector.get_window_length() == 1);
    fixture.feed({1.0F, 0.0F});
    fixture.feed({}, 3);
    CHECK(fixture.accepted.empty());
    fixture.feed({1.0F, 0.0F});
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 1);
    CHECK(fixture.accepted[0].end == 4);
}

TEST_CASE("null intervals report absolute half-open crossings across chunks",
          "[null-symbol-detector]") {
    const auto capacity = GENERATE(1, 7, 512, 4096, 5400);
    // Single-sample smoothing gives exact input-edge crossings.
    std::vector<std::complex<float>> samples(5400, {1.0F, 0.0F});
    std::fill(samples.begin() + 7, samples.begin() + 2663, std::complex<float>{});
    std::fill(samples.begin() + 2700, samples.begin() + 5356, std::complex<float>{});
    DetectorFixture fixture({1, 1.0, 0.1, 0.2});
    fixture.feed_chunks(samples, capacity);
    REQUIRE(fixture.accepted.size() == 2);
    CHECK(fixture.accepted[0].start == 7);
    CHECK(fixture.accepted[0].end == 2663);
    CHECK(fixture.accepted[1].start == 2700);
    CHECK(fixture.accepted[1].end == 5356);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("incomplete candidate at EOF produces no accepted or rejected interval",
          "[null-symbol-detector]") {
    DetectorFixture fixture({1, 1.0, 0.1, 0.2});
    fixture.feed({1.0F, 0.0F});
    fixture.feed({}, 2656);
    CHECK(fixture.accepted.empty());
    CHECK(fixture.rejected.empty());
}

TEST_CASE("smoothed short dip is rejected once and the following null is accepted",
          "[null-symbol-detector]") {
    DetectorFixture fixture({4, 0.01, 0.1, 0.2, 8, 12});
    fixture.feed({1.0F, 0.0F}, 32);
    fixture.feed({}, 6);
    fixture.feed({1.0F, 0.0F}, 32);
    REQUIRE(fixture.accepted.empty());
    REQUIRE(fixture.rejected.size() == 1);
    CHECK(fixture.rejected[0].start == 35);
    CHECK(fixture.rejected[0].end == 38);
    CHECK(fixture.rejected[0].reject_reason == dab::CandidateRejectionReason::TOO_SHORT);
    fixture.feed({}, 12);
    fixture.feed({1.0F, 0.0F}, 4);
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 73);
    CHECK(fixture.accepted[0].end == 82);
    CHECK(fixture.rejected.size() == 1);
}

TEST_CASE("hysteresis preserves a candidate and freezes its reference", "[null-symbol-detector]") {
    DetectorFixture fixture({4, 0.01, 0.125, 0.5, 1, 500});
    fixture.feed({1.0F, 0.0F}, 32);
    fixture.feed({}, 4);
    // Power 0.25 is between entry and exit ratios of the surrounding unit power.
    // If the reference adapted during this hold, the exit threshold would fall
    // below the hold power, causing a premature exit.
    fixture.feed({0.5F, 0.0F}, 200);
    CHECK(fixture.accepted.empty());
    CHECK(fixture.rejected.empty());
    fixture.feed({1.0F, 0.0F}, 2);
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 35);
    CHECK(fixture.accepted[0].end == 237);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("power between entry and exit thresholds does not start a candidate",
          "[null-symbol-detector]") {
    DetectorFixture fixture({4, 0.0001, 0.125, 0.5, 1, 500});
    fixture.feed({1.0F, 0.0F}, 32);
    fixture.feed({0.5F, 0.0F}, 100);
    fixture.feed({1.0F, 0.0F}, 4);
    CHECK(fixture.accepted.empty());
    CHECK(fixture.rejected.empty());
}

TEST_CASE("comparison uses the old reference before a fast update", "[null-symbol-detector]") {
    DetectorFixture fixture({1, 1.0, 0.2, 0.4, 2, 4});
    fixture.feed({1.0F, 0.0F});
    // A nonzero dip would update the reference to itself with alpha=1. Entry
    // must be tested first, then frozen, to retain this interval.
    fixture.feed({0.25F, 0.0F}, 3);
    fixture.feed({1.0F, 0.0F});
    REQUIRE(fixture.accepted.size() == 1);
    CHECK(fixture.accepted[0].start == 1);
    CHECK(fixture.accepted[0].end == 4);
    CHECK(fixture.rejected.empty());
}

TEST_CASE("fixed-seed stationary noise has no accepted nulls with default settings",
          "[null-symbol-detector]") {
    DetectorFixture fixture({});
    std::mt19937 engine(12345);
    // Fixed integer-to-float mapping avoids implementation-dependent normal
    // distribution generators. Uniform zero-mean noise exercises both I and Q.
    const auto component = [&]() {
        return static_cast<float>(static_cast<int>(engine() % 257) - 128) / 512.0F;
    };
    for (int n = 0; n < 400000; ++n) {
        const float i = component();
        const float q = component();
        fixture.feed({i, q});
    }
    CHECK(fixture.accepted.empty());
    CHECK(fixture.rejected.empty());
}

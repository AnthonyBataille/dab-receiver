#include "frame_periodicity_validator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>

using dab::FramePeriodicityValidator;
using dab::FramePeriodicityValidatorParams;
using dab::FrameSpacingStatus;
using dab::ValidatorState;

TEST_CASE("validator rejects invalid parameters", "[frame-periodicity-validator]") {
    const auto params = GENERATE(
        FramePeriodicityValidatorParams{0, 0, 1}, FramePeriodicityValidatorParams{100, 100, 1},
        FramePeriodicityValidatorParams{100, 101, 1}, FramePeriodicityValidatorParams{100, 0, 0});
    REQUIRE_THROWS_AS(FramePeriodicityValidator{params}, std::invalid_argument);
}

TEST_CASE("default acquisition needs six nulls and caps the matching count",
          "[frame-periodicity-validator]") {
    FramePeriodicityValidator validator;
    CHECK(validator.get_state() == ValidatorState::SEARCHING);
    CHECK(validator.get_consecutive_matches() == 0);
    const auto first = validator.consume({0, 2656});
    CHECK(first.candidate.start == 0);
    CHECK(first.candidate.end == 2656);
    CHECK_FALSE(first.spacing_samples);
    CHECK(first.spacing_status == FrameSpacingStatus::FIRST_CANDIDATE);
    CHECK(first.state == ValidatorState::CONFIRMING);
    CHECK(first.consecutive_matches == 0);

    // Explicit observations and expected counts, independent of state-machine logic.
    const std::uint64_t starts[] = {196608, 393216, 589824, 786432, 983040, 1179648};
    const std::size_t counts[] = {1, 2, 3, 4, 5, 5};
    const ValidatorState states[] = {ValidatorState::CONFIRMING, ValidatorState::CONFIRMING,
                                     ValidatorState::CONFIRMING, ValidatorState::CONFIRMING,
                                     ValidatorState::TRACKING,   ValidatorState::TRACKING};
    for (std::size_t i = 0; i < 6; ++i) {
        const auto result = validator.consume({starts[i], starts[i] + 2656});
        REQUIRE(result.spacing_samples);
        CHECK(*result.spacing_samples == 196608);
        CHECK(result.spacing_status == FrameSpacingStatus::MATCH);
        CHECK(result.consecutive_matches == counts[i]);
        CHECK(result.state == states[i]);
        CHECK(validator.get_state() == result.state);
        CHECK(validator.get_consecutive_matches() == result.consecutive_matches);
    }
}

TEST_CASE("spacing tolerance is inclusive and uses starts rather than ends",
          "[frame-periodicity-validator]") {
    const auto spacing = GENERATE(89ULL, 90ULL, 100ULL, 110ULL, 111ULL);
    FramePeriodicityValidator validator({100, 10, 1});
    (void)validator.consume({0, 5});
    const auto result = validator.consume({spacing, spacing + 30});
    REQUIRE(result.spacing_samples);
    CHECK(*result.spacing_samples == spacing);
    const bool matches = spacing == 90 || spacing == 100 || spacing == 110;
    CHECK(result.spacing_status ==
          (matches ? FrameSpacingStatus::MATCH : FrameSpacingStatus::MISMATCH));
    CHECK(result.state == (matches ? ValidatorState::TRACKING : ValidatorState::CONFIRMING));
}

TEST_CASE("zero tolerance accepts exact spacing only", "[frame-periodicity-validator]") {
    const auto spacing = GENERATE(99ULL, 100ULL, 101ULL);
    FramePeriodicityValidator validator({100, 0, 1});
    (void)validator.consume({0, 1});
    const auto result = validator.consume({spacing, spacing + 1});
    CHECK(result.spacing_status ==
          (spacing == 100 ? FrameSpacingStatus::MATCH : FrameSpacingStatus::MISMATCH));
}

TEST_CASE("mismatch restarts confirmation from the current candidate",
          "[frame-periodicity-validator]") {
    const bool already_tracking = GENERATE(false, true);
    // Extra candidate, missed null, and ordinary off-period candidate.
    const auto gap = GENERATE(50ULL, 200ULL, 120ULL);
    FramePeriodicityValidator validator({100, 0, 2});
    (void)validator.consume({0, 1});
    (void)validator.consume({100, 101});
    std::uint64_t previous = 100;
    if (already_tracking) {
        REQUIRE(validator.consume({200, 201}).state == ValidatorState::TRACKING);
        previous = 200;
    }
    const auto restart = previous + gap;
    const auto mismatch = validator.consume({restart, restart + 1});
    CHECK(mismatch.spacing_status == FrameSpacingStatus::MISMATCH);
    CHECK(mismatch.consecutive_matches == 0);
    CHECK(mismatch.state == ValidatorState::CONFIRMING);
    const auto next = validator.consume({restart + 100, restart + 101});
    CHECK(next.spacing_status == FrameSpacingStatus::MATCH);
    CHECK(next.consecutive_matches == 1);
    CHECK(next.state == ValidatorState::CONFIRMING);
    CHECK(validator.consume({restart + 200, restart + 201}).state == ValidatorState::TRACKING);
}

TEST_CASE("invalid intervals leave state and previous candidate unchanged",
          "[frame-periodicity-validator]") {
    const auto invalid = GENERATE(dab::NullSymbolInterval{30, 30}, dab::NullSymbolInterval{40, 39},
                                  dab::NullSymbolInterval{19, 25}, dab::NullSymbolInterval{5, 6},
                                  dab::NullSymbolInterval{10, 20});
    const auto required = GENERATE(std::size_t{1}, std::size_t{2});
    FramePeriodicityValidator validator({10, 0, required});
    (void)validator.consume({0, 10});
    (void)validator.consume({10, 20}); // Touching half-open intervals are valid.
    const auto state = validator.get_state();
    REQUIRE_THROWS_AS(validator.consume(invalid), std::invalid_argument);
    CHECK(validator.get_state() == state);
    CHECK(validator.get_consecutive_matches() == 1);
    const auto next = validator.consume({20, 30});
    CHECK(next.spacing_samples == 10);
    CHECK(next.spacing_status == FrameSpacingStatus::MATCH);
    CHECK(next.state == ValidatorState::TRACKING);
}

TEST_CASE("invalid first interval does not seed acquisition", "[frame-periodicity-validator]") {
    FramePeriodicityValidator validator;
    REQUIRE_THROWS_AS(validator.consume({5, 5}), std::invalid_argument);
    CHECK(validator.get_state() == ValidatorState::SEARCHING);
    CHECK(validator.get_consecutive_matches() == 0);
    CHECK(validator.consume({0, 1}).spacing_status == FrameSpacingStatus::FIRST_CANDIDATE);
}

TEST_CASE("reset clears acquisition history and preserves copied configuration",
          "[frame-periodicity-validator]") {
    FramePeriodicityValidatorParams params{10, 0, 1};
    FramePeriodicityValidator validator(params);
    params.expected_frame_period = 100;
    params.required_consecutive_matches = 5;
    const bool start_tracking = GENERATE(false, true);
    (void)validator.consume({100, 101});
    if (start_tracking) {
        REQUIRE(validator.consume({110, 111}).state == ValidatorState::TRACKING);
    }
    validator.reset();
    validator.reset();
    CHECK(validator.get_state() == ValidatorState::SEARCHING);
    CHECK(validator.get_consecutive_matches() == 0);
    const auto first = validator.consume({0, 1});
    CHECK_FALSE(first.spacing_samples);
    CHECK(first.spacing_status == FrameSpacingStatus::FIRST_CANDIDATE);
    CHECK(first.consecutive_matches == 0);
    CHECK(validator.consume({10, 11}).state == ValidatorState::TRACKING);
}

TEST_CASE("large indices and periods do not overflow spacing comparisons",
          "[frame-periodicity-validator]") {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    SECTION("absolute indices above signed range") {
        FramePeriodicityValidator validator({100, 0, 1});
        (void)validator.consume({maximum - 101, maximum - 100});
        const auto result = validator.consume({maximum - 1, maximum});
        CHECK(result.spacing_samples == 100);
        CHECK(result.state == ValidatorState::TRACKING);
    }
    SECTION("expected period plus tolerance would overflow") {
        FramePeriodicityValidator validator({maximum, maximum - 1, 1});
        (void)validator.consume({0, 1});
        CHECK(validator.consume({maximum - 1, maximum}).state == ValidatorState::TRACKING);
    }
    SECTION("spacing too large for signed subtraction") {
        FramePeriodicityValidator validator({1, 0, 1});
        (void)validator.consume({0, 1});
        CHECK(validator.consume({maximum - 1, maximum}).spacing_status ==
              FrameSpacingStatus::MISMATCH);
    }
}

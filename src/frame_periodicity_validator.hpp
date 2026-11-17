#pragma once

#include "null_symbol_detector.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dab {

struct FramePeriodicityValidatorParams {
    // Mode I at 2.048 MS/s: 96 ms = 196608 samples.
    // ETSI EN 300 401 V1.3.3, clause 14.2, table 43:
    // https://www.etsi.org/deliver/etsi_en/300400_300499/300401/01.03.03_60/en_300401v010303p.pdf
    // All sample counts must use the same sample rate as the input intervals.
    std::uint64_t expected_frame_period = 196'608;
    // Experimental inclusive absolute error limit: about 0.5% of 196608.
    std::uint64_t spacing_tolerance = 983;
    // Count matching spacings, not intervals: five matches need six nulls.
    std::size_t required_consecutive_matches = 5;
};

enum class ValidatorState { SEARCHING = 0, CONFIRMING = 1, TRACKING = 2 };

enum class FrameSpacingStatus { FIRST_CANDIDATE, MATCH, MISMATCH };

// One result per input interval. State and count describe the validator AFTER
// consuming this interval. The caller owns any retained diagnostic history.
struct FramePeriodicityResult {
    NullSymbolInterval candidate{};
    // Start-to-start spacing from the previous candidate; absent for the first.
    std::optional<std::uint64_t> spacing_samples;
    FrameSpacingStatus spacing_status = FrameSpacingStatus::FIRST_CANDIDATE;
    ValidatorState state = ValidatorState::SEARCHING;
    // Saturates at required_consecutive_matches while tracking.
    std::size_t consecutive_matches = 0;
};

class FramePeriodicityValidator {
  public:
    // Create a validator with validated parameters.
    explicit FramePeriodicityValidator(FramePeriodicityValidatorParams params = {});

    // Process a null interval and return the updated spacing and acquisition status.
    [[nodiscard]] FramePeriodicityResult consume(NullSymbolInterval candidate);

    // Clear acquisition history while preserving parameters.
    void reset();

    // Return the current acquisition state.
    [[nodiscard]] ValidatorState get_state() const;
    // Return the number of consecutive matching spacings.
    [[nodiscard]] std::size_t get_consecutive_matches() const;

  private:
    const FramePeriodicityValidatorParams params_;
    ValidatorState state_ = ValidatorState::SEARCHING;
    // Empty until the first interval; sample index zero remains a valid start.
    std::optional<NullSymbolInterval> last_candidate_;
    std::size_t consecutive_matches_ = 0;
};

} // namespace dab

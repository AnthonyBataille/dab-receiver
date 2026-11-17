#include "frame_periodicity_validator.hpp"

#include <stdexcept>

namespace dab {
namespace {
FramePeriodicityValidatorParams validate_params(FramePeriodicityValidatorParams params) {
    if (params.expected_frame_period == 0) {
        throw std::invalid_argument("Expected frame period must be positive");
    }
    if (params.spacing_tolerance >= params.expected_frame_period) {
        throw std::invalid_argument("Spacing tolerance must be less than the frame period");
    }
    if (params.required_consecutive_matches == 0) {
        throw std::invalid_argument("Required consecutive matches must be positive");
    }
    return params;
}
} // namespace

FramePeriodicityValidator::FramePeriodicityValidator(FramePeriodicityValidatorParams params)
    : params_(validate_params(params)) {}

FramePeriodicityResult FramePeriodicityValidator::consume(NullSymbolInterval candidate) {
    if (candidate.start >= candidate.end) {
        throw std::invalid_argument("Null interval must have start < end");
    }
    if (last_candidate_ && candidate.start < last_candidate_->end) {
        throw std::invalid_argument("Null intervals must be ordered and non-overlapping");
    }

    FramePeriodicityResult result{};
    result.candidate = candidate;
    if (!last_candidate_) {
        state_ = ValidatorState::CONFIRMING;
        consecutive_matches_ = 0;
    } else {
        const auto spacing = candidate.start - last_candidate_->start;
        result.spacing_samples = spacing;
        // Subtract smaller from larger to avoid unsigned underflow. Avoid adding
        // tolerance to the expected period, which could overflow for custom values.
        const auto error = spacing >= params_.expected_frame_period
                               ? spacing - params_.expected_frame_period
                               : params_.expected_frame_period - spacing;
        if (error <= params_.spacing_tolerance) {
            result.spacing_status = FrameSpacingStatus::MATCH;
            if (consecutive_matches_ < params_.required_consecutive_matches) {
                ++consecutive_matches_;
            }
            state_ = consecutive_matches_ == params_.required_consecutive_matches
                         ? ValidatorState::TRACKING
                         : ValidatorState::CONFIRMING;
        } else {
            result.spacing_status = FrameSpacingStatus::MISMATCH;
            consecutive_matches_ = 0;
            state_ = ValidatorState::CONFIRMING;
        }
    }

    last_candidate_ = candidate;
    result.state = state_;
    result.consecutive_matches = consecutive_matches_;
    return result;
}

void FramePeriodicityValidator::reset() {
    last_candidate_.reset();
    consecutive_matches_ = 0;
    state_ = ValidatorState::SEARCHING;
}

ValidatorState FramePeriodicityValidator::get_state() const {
    return state_;
}

std::size_t FramePeriodicityValidator::get_consecutive_matches() const {
    return consecutive_matches_;
}
} // namespace dab

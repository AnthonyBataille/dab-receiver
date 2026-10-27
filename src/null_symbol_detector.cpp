#include "null_symbol_detector.hpp"

#include <complex>
#include <cmath>
#include <cstddef>
#include <vector>
#include <stdexcept>

namespace dab {
namespace {
NullSymbolDetectorParams validate_params(NullSymbolDetectorParams params) {
    if (params.window_length == 0) {
        throw std::invalid_argument("Null detector window length must be positive");
    }
    if (!std::isfinite(params.reference_power_update_rate) ||
        params.reference_power_update_rate <= 0.0 || params.reference_power_update_rate > 1.0) {
        throw std::invalid_argument(
            "Null detector reference update rate must be finite and in (0, 1]");
    }
    if (!std::isfinite(params.threshold_in) || !std::isfinite(params.threshold_out) ||
        params.threshold_in <= 0.0 || params.threshold_in >= params.threshold_out ||
        params.threshold_out >= 1.0) {
        throw std::invalid_argument("Null detector thresholds must satisfy 0 < entry < exit < 1");
    }
    if (params.min_candidate_duration == 0 ||
        params.max_candidate_duration < params.min_candidate_duration) {
        throw std::invalid_argument("Null detector duration bounds must satisfy 0 < min <= max");
    }
    return params;
}
} // namespace

NullSymbolDetector::NullSymbolDetector(NullSymbolDetectorParams params)
    : params_(validate_params(params)), power_buffer_(params_.window_length),
      window_sample_index_(0), power_sum_(0.0), buffer_full_(false), power_reference_(0.0),
      reference_ready_(false), acquisition_signal_samples_(0), global_sample_counter_(0),
      candidate_null_(false), candidate_start_index_(0), recovering_(false),
      recovery_signal_samples_(0) {}

std::size_t NullSymbolDetector::get_window_length() const {
    return params_.window_length;
}

void NullSymbolDetector::add_power(const std::complex<float> input_sample) {
    const double input_sample_i = input_sample.real();
    const double input_sample_q = input_sample.imag();

    const double input_sample_power =
        input_sample_i * input_sample_i + input_sample_q * input_sample_q;

    if (buffer_full_) {
        power_sum_ -= power_buffer_[window_sample_index_];
    }

    power_buffer_[window_sample_index_] = input_sample_power;
    power_sum_ += input_sample_power;
    if (!buffer_full_ && window_sample_index_ >= params_.window_length - 1) {
        buffer_full_ = true;
    }
    window_sample_index_ = (window_sample_index_ + 1) % params_.window_length;
    ++global_sample_counter_;
}

void NullSymbolDetector::update_reference_power() {
    if (!buffer_full_) {
        return;
    }
    if (!reference_ready_) {
        if (power_sum_ <= 0.0) {
            acquisition_signal_samples_ = 0;
            power_reference_ = 0.0;
            return;
        }
        if (power_reference_ <= 0.0) {
            power_reference_ = power_sum_;
        }
        // Compare to the previous reference before allowing this window to adapt it.
        if (power_sum_ > params_.threshold_out * power_reference_) {
            ++acquisition_signal_samples_;
        } else {
            acquisition_signal_samples_ = 0;
        }
        power_reference_ += params_.reference_power_update_rate * (power_sum_ - power_reference_);
        if (acquisition_signal_samples_ >= params_.window_length) {
            power_reference_ = power_sum_;
            reference_ready_ = true;
        }
        return;
    }
    if (!candidate_null_) {
        power_reference_ += params_.reference_power_update_rate * (power_sum_ - power_reference_);
    }
}

std::optional<NullSymbolInterval>
NullSymbolDetector::update_candidate_state(std::optional<RejectedCandidateInfo>* rejection) {
    if (rejection) {
        rejection->reset();
    }
    if (!buffer_full_ || !reference_ready_ || global_sample_counter_ == 0) {
        return std::nullopt;
    }
    std::optional<NullSymbolInterval> interval;
    const auto sample_index = global_sample_counter_ - 1;
    if (recovering_) {
        if (power_sum_ > 0.0 && power_sum_ > params_.threshold_out * power_reference_) {
            ++recovery_signal_samples_;
            if (recovery_signal_samples_ >= params_.window_length) {
                recovering_ = false;
                recovery_signal_samples_ = 0;
                power_reference_ = power_sum_;
            }
        } else {
            recovery_signal_samples_ = 0;
        }
        return std::nullopt;
    }
    if (!candidate_null_) {
        if (power_sum_ < params_.threshold_in * power_reference_) {
            candidate_null_ = true;
            candidate_start_index_ = sample_index;
        }
    } else {
        const auto candidate_duration = sample_index - candidate_start_index_;
        if (power_sum_ > params_.threshold_out * power_reference_) {

            if (candidate_duration >= params_.min_candidate_duration &&
                candidate_duration <= params_.max_candidate_duration) {
                interval = NullSymbolInterval{candidate_start_index_, sample_index};
            } else {
                if (rejection) {
                    *rejection = RejectedCandidateInfo{candidate_start_index_, sample_index,
                                                       CandidateRejectionReason::TOO_SHORT};
                }
            }
            candidate_null_ = false;
            candidate_start_index_ = 0;
            // Current low sample has offset max when the interval length is max+1.
            // Comparing the offset avoids adding one to a potentially large duration.
        } else if (candidate_duration >= params_.max_candidate_duration) {
            if (rejection) {
                *rejection = RejectedCandidateInfo{candidate_start_index_, sample_index,
                                                   CandidateRejectionReason::TOO_LONG};
            }
            candidate_null_ = false;
            candidate_start_index_ = 0;
            recovering_ = true;
            recovery_signal_samples_ = 0;
        }
    }
    return interval;
}
} // namespace dab

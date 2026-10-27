#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dab {
// Experimental starting values for 2.048 MS/s input, not capture-validated tuning.
// Reference update rate is per sample; thresholds are linear power/reference ratios.
struct NullSymbolDetectorParams {
    std::size_t window_length = 128; // Samples in the smoothing buffer; must be positive.
    double reference_power_update_rate = 0.0001; // Finite, in (0, 1].
    double threshold_in = 0.2;                   // Finite, 0 < entry < exit < 1.
    double threshold_out = 0.4;
    // Inclusive duration bounds in samples: require 0 < min <= max.
    // Mode I nominal null: 2656 samples at 2.048 MS/s (ETSI EN 300 401
    // V1.3.3, clause 14.2, table 43):
    // https://www.etsi.org/deliver/etsi_en/300400_300499/300401/01.03.03_60/en_300401v010303p.pdf
    // Experimental +/-10% allows crossing shifts from smoothing/channel effects.
    // The default 128-sample window is smaller than this 266-sample margin;
    // changing the window/thresholds requires revalidating the duration bounds.
    std::uint64_t min_candidate_duration = 2390;
    std::uint64_t max_candidate_duration = 2922;
};

// Approximate threshold crossings in zero-based absolute sample indices: [start, end).
// These are not precise PRS or FFT timing estimates.
struct NullSymbolInterval {
    std::uint64_t start;
    std::uint64_t end;
};

enum class CandidateRejectionReason : unsigned int { TOO_SHORT = 1, TOO_LONG = 2 };

struct RejectedCandidateInfo {
    std::uint64_t start;
    std::uint64_t end;
    CandidateRejectionReason reject_reason;
};

class NullSymbolDetector {
  public:
    // Copies and validates configuration; invalid values throw std::invalid_argument.
    explicit NullSymbolDetector(NullSymbolDetectorParams params = {});
    std::size_t get_window_length() const;
    void add_power(const std::complex<float> input_sample);
    // Call once after each add_power(), before updating the reference. Returns an
    // interval only on the exit sample of a duration-qualified candidate. The exit
    // sample is excluded. Incomplete/rejected candidates produce no event.
    // After timeout, reference updates continue and candidates are suppressed until
    // window_length consecutive positive-power windows exceed the exit threshold.
    // Recovery reseeds the reference from the current window and emits no interval.
    std::optional<NullSymbolInterval>
    update_candidate_state(std::optional<RejectedCandidateInfo>* rejection = nullptr);
    // Ignore partial windows. At startup, seed from a positive full window and
    // require window_length consecutive positive windows above the exit ratio.
    // Detection begins on the sample after acquisition completes.
    void update_reference_power();

  private:
    const NullSymbolDetectorParams params_;
    // Container sizes and offsets use the platform's native indexing type.
    std::vector<double> power_buffer_;
    std::size_t window_sample_index_;
    double power_sum_;
    bool buffer_full_;
    double power_reference_;
    bool reference_ready_;
    std::size_t acquisition_signal_samples_;
    std::uint64_t global_sample_counter_;
    bool candidate_null_;
    std::uint64_t candidate_start_index_;
    bool recovering_;
    std::size_t recovery_signal_samples_;
};
} // namespace dab

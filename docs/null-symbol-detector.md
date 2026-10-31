# Null-symbol detector

## Purpose

`dab::NullSymbolDetector` finds low-power intervals in continuous
`std::complex<float>` IQ input at 2.048 MS/s. It smooths sample power, compares
it with a signal-power reference, and accepts intervals of suitable duration.

Results are approximate threshold crossings, not precise PRS or FFT timing.
Frame periodicity validation is a separate stage. The detector has no file I/O
or console output.

## Usage

Include `null_symbol_detector.hpp`. Create one detector per continuous stream
and keep it across input blocks. For each sample, call these methods once,
in this order:

```cpp
dab::NullSymbolDetector detector; // Experimental defaults.

for (const auto sample : block) {
    detector.add_power(sample);
    auto interval = detector.update_candidate_state();
    detector.update_reference_power();
    if (interval) {
        // Consume interval->start and interval->end.
    }
}
```

Intervals use zero-based 64-bit absolute indices: `[start, end)`, with duration
`end - start`. An incomplete candidate at EOF produces no interval.

The offline runner stores accepted intervals in `IqSummary::null_symbols`.
`print_iq_summary` lists them after the recording summary, with start, end,
duration, and start-to-start spacing in samples. The first spacing is empty;
the list is omitted when there are no accepted candidates.

For optional rejection diagnostics, pass a pointer to
`std::optional<dab::RejectedCandidateInfo>`. The output is cleared on every
call and filled only for a rejection on that sample. The caller owns any history.

## Configuration

Pass `NullSymbolDetectorParams` to the constructor. It validates and stores a
copy. Defaults are experimental:

| Parameter | Default |
| --- | ---: |
| Smoothing window | 128 samples |
| Reference update rate | 0.0001 per sample |
| Entry / exit power ratios | 0.2 / 0.4 |
| Accepted duration, inclusive | 2390-2922 samples |

The duration range allows about +/-10% around Mode I's nominal 2656 samples.
Recheck this range when changing smoothing or thresholds. See
[ETSI EN 300 401 V1.3.3, clause 14.2, table 43](https://www.etsi.org/deliver/etsi_en/300400_300499/300401/01.03.03_60/en_300401v010303p.pdf).

## Implementation details

- **Power:** `I*I + Q*Q`, accumulated in a circular buffer with a double-precision
  running sum.
- **Startup:** partial windows are ignored. The first positive full window seeds
  the reference. Detection waits for a window-length run above the exit ratio.
  Zero input stays unready; no leading partial-null interval is inferred.
- **Detection:** compare against the existing reference before updating it.
  Enter below the entry ratio; exit above the higher exit ratio. Freeze the
  reference while a candidate is active.
- **Rejection and recovery:** short candidates are discarded at exit. An overlong
  candidate is rejected immediately and reference updates resume. Recovery
  suppresses new candidates until a window-length run of positive power exceeds
  the exit ratio, then reseeds the reference.
- **Limit:** a power baseline does not identify DAB or distinguish signal from
  noise. Periodicity and PRS processing provide later confirmation.

Source: `src/null_symbol_detector.hpp/.cpp`.
Tests: `tests/null_symbol_detector_test.cpp`.

from pathlib import Path
from typing import TypedDict

import matplotlib.pyplot as plt
import numpy as np
import numpy.typing as npt

ComplexArray = npt.NDArray[np.complex128]
RealArray = npt.NDArray[np.float64]
IntArray = npt.NDArray[np.intp]


class TimingResult(TypedDict):
    offset: int
    is_safe: bool
    model_error: float
    channel_only_evm: float
    corrected_evm: float
    channel_equalized: ComplexArray
    timing_corrected: ComplexArray


class ChannelResult(TypedDict):
    safe_lower: int
    rows: list[TimingResult]


FFT_SIZE = 64
CP_LENGTH = 16
BLOCK_LENGTH = FFT_SIZE + CP_LENGTH
NUM_SYMBOLS = 500
SAMPLE_RATE = 64_000.0

ACTIVE_CARRIERS = np.concatenate(
    (
        np.arange(-8, 0),
        np.arange(1, 9),
    )
)
ACTIVE_BINS = ACTIVE_CARRIERS % FFT_SIZE
NUM_ACTIVE = len(ACTIVE_BINS)

TIMING_OFFSETS = np.arange(-20, 9)
REPORT_OFFSETS = (-20, -17, -16, -12, -9, -8, -4, -2, 0, 1, 4, 8)

ECHO_DELAY = 8
ECHO_AMPLITUDE = 0.5

TOLERANCE = 1e-12
MIN_OUTSIDE_EVM = 1e-3

RNG = np.random.default_rng(2029)
OUTPUT_PATH = Path(__file__).resolve().with_suffix(".png")


def extract_fft(
    received_stream: ComplexArray, nominal_starts: IntArray, offset: int
) -> ComplexArray:
    """Take N samples per symbol, starting 'offset' samples from nominal."""
    indices = nominal_starts[:, None] + offset + np.arange(FFT_SIZE)[None, :]

    assert indices.min() >= 0, "FFT window starts before the recording."
    assert indices.max() < received_stream.size, "FFT window ends after the recording."

    windows = received_stream[indices]
    return np.fft.fft(windows, axis=1)


def calculate_evm(received: ComplexArray, reference: ComplexArray) -> float:
    error_power = np.mean(np.abs(received - reference) ** 2)
    reference_power = np.mean(np.abs(reference) ** 2)
    return float(np.sqrt(error_power / reference_power))


def main() -> None:
    direct_channel = np.array([1.0])

    echo_channel = np.zeros(ECHO_DELAY + 1)
    echo_channel[0] = 1.0
    echo_channel[ECHO_DELAY] = ECHO_AMPLITUDE

    CHANNELS: dict[str, RealArray] = {
        "Direct": direct_channel,
        "Echo": echo_channel,
    }

    total_symbols = NUM_SYMBOLS + 2

    bits = RNG.integers(
        0,
        2,
        size=(total_symbols, NUM_ACTIVE, 2),
        dtype=np.int8,
    )

    qpsk_symbols = ((1 - 2 * bits[..., 0]) + 1j * (1 - 2 * bits[..., 1])) / np.sqrt(2.0)

    frequency_grid = np.zeros(
        (total_symbols, FFT_SIZE),
        dtype=np.complex128,
    )
    frequency_grid[:, ACTIVE_BINS] = qpsk_symbols

    useful_samples = np.fft.ifft(frequency_grid, axis=1)

    tx_blocks = np.concatenate(
        (useful_samples[:, -CP_LENGTH:], useful_samples),
        axis=1,
    )
    tx_stream = tx_blocks.ravel()

    # Evaluate only the interior symbols.
    evaluated_symbols = np.arange(1, NUM_SYMBOLS + 1)

    reference_grid = frequency_grid[evaluated_symbols]
    reference_symbols = qpsk_symbols[evaluated_symbols]

    nominal_starts = evaluated_symbols * BLOCK_LENGTH + CP_LENGTH

    print(f"FFT size: {FFT_SIZE}")
    print(f"Active carriers: {NUM_ACTIVE}")
    print(f"Evaluated OFDM symbols: {NUM_SYMBOLS}")
    print(f"CP length: {CP_LENGTH} samples")
    print(f"Sample rate: {SAMPLE_RATE / 1_000:.1f} ksample/s")
    print(f"Echo delay: {ECHO_DELAY} samples")
    print(f"Echo amplitude: {ECHO_AMPLITUDE}")

    clean_grid = extract_fft(tx_stream, nominal_starts, offset=0)
    clean_error = float(np.max(np.abs(clean_grid - reference_grid)))

    assert clean_error < TOLERANCE, "Clean OFDM round trip failed."

    print(f"Ideal round-trip error: {clean_error:.3e}")

    results: dict[str, ChannelResult] = {}

    for channel_name, impulse_response in CHANNELS.items():
        received_stream = np.convolve(
            tx_stream,
            impulse_response,
            mode="full",
        )

        channel_response = np.fft.fft(
            impulse_response,
            n=FFT_SIZE,
        )
        active_response = channel_response[ACTIVE_BINS]

        assert (
            np.min(np.abs(active_response)) > 1e-10
        ), "Cannot safely divide by a zero channel response."

        max_delay = int(np.flatnonzero(np.abs(impulse_response) > 0.0)[-1])
        safe_lower = max_delay - CP_LENGTH

        rows: list[TimingResult] = []

        for offset_value in TIMING_OFFSETS:
            offset = int(offset_value)

            received_grid = extract_fft(received_stream, nominal_starts, offset)
            received_symbols = received_grid[:, ACTIVE_BINS]

            # Full-bin prediction, valid inside the safe interval.
            full_phase = np.exp(2j * np.pi * np.arange(FFT_SIZE) * offset / FFT_SIZE)
            predicted_grid = (
                reference_grid * channel_response[None, :] * full_phase[None, :]
            )

            model_error = float(np.max(np.abs(received_grid - predicted_grid)))

            # Signed carrier indices give the same factors on active bins.
            timing_phase = np.exp(2j * np.pi * ACTIVE_CARRIERS * offset / FFT_SIZE)

            channel_equalized = received_symbols / active_response[None, :]
            timing_corrected = channel_equalized * np.conj(timing_phase)[None, :]

            channel_only_evm = calculate_evm(
                channel_equalized,
                reference_symbols,
            )
            corrected_evm = calculate_evm(
                timing_corrected,
                reference_symbols,
            )

            is_safe = safe_lower <= offset <= 0

            if is_safe:
                assert (
                    model_error < TOLERANCE
                ), f"{channel_name}, d={offset}: safe-window model failed."
                assert (
                    corrected_evm < TOLERANCE
                ), f"{channel_name}, d={offset}: recovery failed."
            else:
                assert corrected_evm > MIN_OUTSIDE_EVM, (
                    f"{channel_name}, d={offset}: "
                    "expected interference outside the safe interval."
                )

            rows.append(
                {
                    "offset": offset,
                    "is_safe": is_safe,
                    "model_error": model_error,
                    "channel_only_evm": channel_only_evm,
                    "corrected_evm": corrected_evm,
                    "channel_equalized": channel_equalized,
                    "timing_corrected": timing_corrected,
                }
            )

        observed_safe = [
            row["offset"] for row in rows if row["corrected_evm"] < TOLERANCE
        ]
        expected_safe = [
            int(offset) for offset in TIMING_OFFSETS if safe_lower <= offset <= 0
        ]

        assert (
            observed_safe == expected_safe
        ), f"{channel_name}: observed safe offsets differ from prediction."

        results[channel_name] = {
            "safe_lower": safe_lower,
            "rows": rows,
        }

        print()
        print(f"{channel_name} channel: " f"predicted safe interval [{safe_lower}, 0]")
        print(
            f"Verified safe offsets: " f"{observed_safe[0]} through {observed_safe[-1]}"
        )
        print("   d  Safe?  Max model error  " "Channel-only EVM  Channel+timing EVM")
        print("-" * 76)

        for row in rows:
            if row["offset"] in REPORT_OFFSETS:
                print(
                    f"{row['offset']:4d}"
                    f"{str(row['is_safe']):>7}"
                    f"{row['model_error']:17.3e}"
                    f"{100 * row['channel_only_evm']:17.6f}%"
                    f"{100 * row['corrected_evm']:19.6f}%"
                )

    PHASE_OFFSET = -2

    def find_row(channel_name: str, offset: int) -> TimingResult:
        return next(
            row for row in results[channel_name]["rows"] if row["offset"] == offset
        )

    phase_row = find_row("Echo", PHASE_OFFSET)

    measured_factors = np.mean(
        phase_row["channel_equalized"] / reference_symbols,
        axis=0,
    )
    expected_factors = np.exp(2j * np.pi * ACTIVE_CARRIERS * PHASE_OFFSET / FFT_SIZE)

    phase_factor_error = float(np.max(np.abs(measured_factors - expected_factors)))
    assert phase_factor_error < TOLERANCE

    carrier_position = int(np.flatnonzero(ACTIVE_CARRIERS == 8)[0])
    measured_phase_deg = float(np.degrees(np.angle(measured_factors[carrier_position])))

    assert abs(measured_phase_deg - (-90.0)) < 1e-10

    print()
    print(
        f"Phase-factor maximum error at d={PHASE_OFFSET}: " f"{phase_factor_error:.3e}"
    )
    print(
        f"Carrier +8 phase at d={PHASE_OFFSET}: "
        f"measured={measured_phase_deg:.6f} deg, "
        "expected=-90.000000 deg"
    )
    print("All timing-offset checks passed.")

    fig = plt.figure(figsize=(14, 9), constrained_layout=True)
    grid = fig.add_gridspec(2, 6)

    evm_ax = fig.add_subplot(grid[0, :3])

    for channel_name, channel_result in results.items():
        offsets = np.array([row["offset"] for row in channel_result["rows"]])
        evm_percent = 100.0 * np.array(
            [row["corrected_evm"] for row in channel_result["rows"]]
        )

        evm_ax.semilogy(
            offsets,
            np.maximum(evm_percent, 1e-10),
            ".-",
            label=(f"{channel_name}: safe " f"[{channel_result['safe_lower']}, 0]"),
        )

    evm_ax.axvline(-16, color="tab:blue", linestyle=":", alpha=0.7)
    evm_ax.axvline(-8, color="tab:orange", linestyle=":", alpha=0.7)
    evm_ax.axvline(0, color="gray", linestyle=":", alpha=0.7)

    evm_ax.set_xlabel("FFT start offset d (samples; negative = early)")
    evm_ax.set_ylabel("Channel + timing corrected EVM (%)")
    evm_ax.set_title("Recovery succeeds within the predicted safe intervals")
    evm_ax.grid(True, which="both", alpha=0.3)
    evm_ax.legend()

    phase_ax = fig.add_subplot(grid[0, 3:])

    expected_phase_deg = np.degrees(
        2.0 * np.pi * ACTIVE_CARRIERS * PHASE_OFFSET / FFT_SIZE
    )
    measured_phases_deg = np.degrees(np.angle(measured_factors))

    phase_ax.plot(
        ACTIVE_CARRIERS,
        expected_phase_deg,
        "-",
        label="Predicted",
    )
    phase_ax.plot(
        ACTIVE_CARRIERS,
        measured_phases_deg,
        "o",
        label="Measured",
    )

    phase_ax.set_xlabel("Logical carrier index")
    phase_ax.set_ylabel("Timing phase shift (degrees)")
    phase_ax.set_title("Echo channel, safe offset d = -2")
    phase_ax.grid(True, alpha=0.3)
    phase_ax.legend()

    safe_row = find_row("Echo", -4)
    unsafe_row = find_row("Echo", -12)

    panels = (
        (
            "Safe d = -4\nChannel correction only",
            safe_row["channel_equalized"],
        ),
        (
            "Safe d = -4\nChannel + timing correction",
            safe_row["timing_corrected"],
        ),
        (
            "Unsafe d = -12\nChannel + timing correction",
            unsafe_row["timing_corrected"],
        ),
    )

    ideal_points = np.array(
        [
            1 + 1j,
            -1 + 1j,
            -1 - 1j,
            1 - 1j,
        ]
    ) / np.sqrt(2.0)

    for column, (title, symbols) in enumerate(panels):
        ax = fig.add_subplot(grid[1, 2 * column : 2 * column + 2])
        points = symbols.ravel()

        ax.scatter(
            points.real,
            points.imag,
            s=6,
            alpha=0.20,
        )
        ax.scatter(
            ideal_points.real,
            ideal_points.imag,
            marker="x",
            s=100,
            linewidths=2,
            color="tab:red",
            zorder=3,
        )

        ax.axhline(0.0, color="gray", linewidth=0.8)
        ax.axvline(0.0, color="gray", linewidth=0.8)
        ax.set_xlim(-2.0, 2.0)
        ax.set_ylim(-2.0, 2.0)
        ax.set_aspect("equal", adjustable="box")
        ax.set_xlabel("In-phase")
        ax.set_ylabel("Quadrature")
        ax.set_title(title)
        ax.grid(True, alpha=0.3)

    fig.suptitle("OFDM timing offset: cyclic-prefix protection and its limits")
    fig.savefig(OUTPUT_PATH, dpi=160)

    print(f"Saved plot to: {OUTPUT_PATH}")
    plt.show()


if __name__ == "__main__":
    main()

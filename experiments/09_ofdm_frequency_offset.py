from pathlib import Path
from typing import TypedDict

import matplotlib.pyplot as plt
import numpy as np
import numpy.typing as npt

ComplexArray = npt.NDArray[np.complex128]


class OffsetResult(TypedDict):
    epsilon: float
    raw_evm: float
    phase_evm: float
    corrected_error: float
    received_symbols: ComplexArray
    phase_symbols: ComplexArray | None
    corrected_symbols: ComplexArray

FFT_SIZE = 64
CP_LENGTH = 16
BLOCK_LENGTH = FFT_SIZE + CP_LENGTH
NUM_SYMBOLS = 200
SAMPLE_RATE = 64_000.0
CARRIER_SPACING = SAMPLE_RATE / FFT_SIZE

ACTIVE_CARRIERS = np.concatenate(
    (
        np.arange(-8, 0),
        np.arange(1, 9),
    )
)
ACTIVE_BINS = ACTIVE_CARRIERS % FFT_SIZE
NUM_ACTIVE = len(ACTIVE_BINS)

OFFSETS = (0.0, 0.01, 0.1, 0.25, 1.0)
DISPLAY_OFFSET = 0.25
TOLERANCE = 1e-12

RNG = np.random.default_rng(2028)
OUTPUT_PATH = Path(__file__).resolve().with_suffix(".png")


def demodulate(received_blocks: ComplexArray) -> ComplexArray:
    """Return all FFT bins using perfect symbol timing."""
    useful = received_blocks[:, CP_LENGTH : CP_LENGTH + FFT_SIZE]
    return np.fft.fft(useful, axis=1)


def calculate_evm(received: ComplexArray, reference: ComplexArray) -> float:
    error_power = np.mean(np.abs(received - reference) ** 2)
    signal_power = np.mean(np.abs(reference) ** 2)
    return float(np.sqrt(error_power / signal_power))


def desired_coefficient(epsilon: float) -> complex:
    local_indices = np.arange(FFT_SIZE)
    return np.mean(np.exp(2j * np.pi * epsilon * local_indices / FFT_SIZE))


def main() -> None:
    bits = RNG.integers(
        0,
        2,
        size=(NUM_SYMBOLS, NUM_ACTIVE, 2),
        dtype=np.int8,
    )

    tx_symbols = ((1 - 2 * bits[..., 0]) + 1j * (1 - 2 * bits[..., 1])) / np.sqrt(2.0)

    frequency_grid = np.zeros(
        (NUM_SYMBOLS, FFT_SIZE),
        dtype=np.complex128,
    )
    frequency_grid[:, ACTIVE_BINS] = tx_symbols

    useful_samples = np.fft.ifft(frequency_grid, axis=1)

    tx_blocks = np.concatenate(
        (useful_samples[:, -CP_LENGTH:], useful_samples),
        axis=1,
    )

    sample_indices = np.arange(tx_blocks.size).reshape(tx_blocks.shape)

    useful_start_indices = np.arange(NUM_SYMBOLS) * BLOCK_LENGTH + CP_LENGTH

    clean_grid = demodulate(tx_blocks)
    clean_error = float(np.max(np.abs(clean_grid - frequency_grid)))

    assert clean_error < TOLERANCE, "Clean OFDM round trip failed."

    print(f"FFT size: {FFT_SIZE}")
    print(f"Active carriers: {NUM_ACTIVE}")
    print(f"OFDM symbols: {NUM_SYMBOLS}")
    print(f"CP length: {CP_LENGTH} samples")
    print(f"Carrier spacing: {CARRIER_SPACING:.1f} Hz")
    print(f"Ideal round-trip error: {clean_error:.3e}")

    results: list[OffsetResult] = []

    for epsilon in OFFSETS:
        oscillator = np.exp(2j * np.pi * epsilon * sample_indices / FFT_SIZE)
        received_blocks = tx_blocks * oscillator

        magnitude_error = float(
            np.max(np.abs(np.abs(received_blocks) - np.abs(tx_blocks)))
        )
        assert (
            magnitude_error < TOLERANCE
        ), "Frequency offset changed sample magnitudes."

        received_grid = demodulate(received_blocks)
        received_symbols = received_grid[:, ACTIVE_BINS]
        raw_evm = calculate_evm(received_symbols, tx_symbols)

        coefficient = desired_coefficient(epsilon)
        desired_factors = (
            np.exp(2j * np.pi * epsilon * useful_start_indices / FFT_SIZE) * coefficient
        )

        phase_symbols = None
        phase_evm = float("nan")

        if abs(coefficient) > 1e-10:
            phase_correction = np.exp(-1j * np.angle(desired_factors))
            phase_symbols = received_symbols * phase_correction[:, None]
            phase_evm = calculate_evm(phase_symbols, tx_symbols)

        corrected_blocks = received_blocks * np.conj(oscillator)
        corrected_grid = demodulate(corrected_blocks)
        corrected_symbols = corrected_grid[:, ACTIVE_BINS]

        corrected_error = float(np.max(np.abs(corrected_grid - frequency_grid)))
        assert (
            corrected_error < TOLERANCE
        ), f"Exact correction failed for epsilon={epsilon}."

        if epsilon == 0.0:
            assert raw_evm < TOLERANCE

        if epsilon in (0.1, 0.25):
            assert (
                phase_evm > 0.01
            ), "Expected residual error after phase-only correction."

        results.append(
            {
                "epsilon": epsilon,
                "raw_evm": raw_evm,
                "phase_evm": phase_evm,
                "corrected_error": corrected_error,
                "received_symbols": received_symbols,
                "phase_symbols": phase_symbols,
                "corrected_symbols": corrected_symbols,
            }
        )

    print()
    print(" epsilon  Offset Hz   Raw EVM   Phase-only EVM" "   Max corrected error")
    print("-" * 75)

    for result in results:
        phase_text = (
            "N/A"
            if np.isnan(result["phase_evm"])
            else f"{100 * result['phase_evm']:.3f}%"
        )

        print(
            f"{result['epsilon']:8.2f}"
            f"{result['epsilon'] * CARRIER_SPACING:11.1f}"
            f"{100 * result['raw_evm']:10.3f}%"
            f"{phase_text:>17}"
            f"{result['corrected_error']:22.3e}"
        )

    PROBE_CARRIER = 3
    PROBE_SYMBOLS = 12
    PROBE_OFFSETS = (0.0, DISPLAY_OFFSET, 1.0)

    probe_grid = np.zeros(
        (PROBE_SYMBOLS, FFT_SIZE),
        dtype=np.complex128,
    )
    probe_grid[:, PROBE_CARRIER] = 1.0

    probe_useful = np.fft.ifft(probe_grid, axis=1)
    probe_blocks = np.concatenate(
        (probe_useful[:, -CP_LENGTH:], probe_useful),
        axis=1,
    )
    probe_indices = np.arange(probe_blocks.size).reshape(probe_blocks.shape)

    probe_outputs: dict[float, ComplexArray] = {}

    print()
    print("Single-carrier checks:")

    for epsilon in PROBE_OFFSETS:
        oscillator = np.exp(2j * np.pi * epsilon * probe_indices / FFT_SIZE)
        output = demodulate(probe_blocks * oscillator)
        probe_outputs[epsilon] = output

        bin_powers = np.abs(output[0]) ** 2
        total_power = float(np.sum(bin_powers))
        peak_bin = int(np.argmax(bin_powers))
        retained_fraction = float(bin_powers[PROBE_CARRIER] / total_power)

        assert abs(total_power - 1.0) < TOLERANCE

        print(
            f"  epsilon={epsilon:.2f}: "
            f"peak bin={peak_bin}, "
            f"fraction in original bin={retained_fraction:.6f}"
        )

        if epsilon in (0.0, 1.0):
            expected_bin = (PROBE_CARRIER + int(epsilon)) % FFT_SIZE

            other_bins = np.arange(FFT_SIZE) != expected_bin

            assert peak_bin == expected_bin
            assert np.max(np.abs(output[0, other_bins])) < TOLERANCE
            assert abs(abs(output[0, expected_bin]) - 1.0) < TOLERANCE

        else:
            expected_fraction = abs(desired_coefficient(epsilon)) ** 2

            assert abs(retained_fraction - expected_fraction) < TOLERANCE
            assert 1.0 - retained_fraction > 0.01

    probe_values = probe_outputs[DISPLAY_OFFSET][:, PROBE_CARRIER]
    measured_phase = np.unwrap(np.angle(probe_values))

    probe_starts = np.arange(PROBE_SYMBOLS) * BLOCK_LENGTH + CP_LENGTH
    predicted_phase = 2.0 * np.pi * DISPLAY_OFFSET * probe_starts / FFT_SIZE + np.angle(
        desired_coefficient(DISPLAY_OFFSET)
    )

    expected_step = 2.0 * np.pi * DISPLAY_OFFSET * BLOCK_LENGTH / FFT_SIZE
    measured_steps = np.diff(measured_phase)

    assert (
        np.max(np.abs(measured_steps - expected_step)) < TOLERANCE
    ), "Phase progression does not match theory."

    print()
    print(
        "Phase advance between useful-symbol starts: "
        f"measured={np.degrees(np.mean(measured_steps)):.6f} deg, "
        f"expected={np.degrees(expected_step):.6f} deg"
    )
    print("All frequency-offset checks passed.")

    selected = next(result for result in results if result["epsilon"] == DISPLAY_OFFSET)
    assert selected["phase_symbols"] is not None

    fig = plt.figure(figsize=(14, 9), constrained_layout=True)
    grid = fig.add_gridspec(2, 6)

    ideal_points = np.array(
        [
            1 + 1j,
            -1 + 1j,
            -1 - 1j,
            1 - 1j,
        ]
    ) / np.sqrt(2.0)

    panels = (
        ("Uncorrected", selected["received_symbols"]),
        ("Phase-only correction", selected["phase_symbols"]),
        ("Exact frequency correction", selected["corrected_symbols"]),
    )

    for column, (title, symbols) in enumerate(panels):
        ax = fig.add_subplot(grid[0, 2 * column : 2 * column + 2])
        points = symbols.ravel()

        ax.scatter(
            points.real,
            points.imag,
            s=7,
            alpha=0.25,
            label="Received",
        )
        ax.scatter(
            ideal_points.real,
            ideal_points.imag,
            marker="x",
            s=100,
            linewidths=2,
            color="tab:red",
            label="Ideal QPSK",
            zorder=3,
        )

        ax.axhline(0.0, color="gray", linewidth=0.8)
        ax.axvline(0.0, color="gray", linewidth=0.8)
        ax.set_xlim(-2.2, 2.2)
        ax.set_ylim(-2.2, 2.2)
        ax.set_aspect("equal", adjustable="box")
        ax.set_xlabel("In-phase")
        ax.set_ylabel("Quadrature")
        ax.set_title(title)
        ax.grid(True, alpha=0.3)

        if column == 0:
            ax.legend(loc="upper right")

    spectrum_ax = fig.add_subplot(grid[1, :3])
    logical_bins = np.arange(-FFT_SIZE // 2, FFT_SIZE // 2)

    for epsilon in PROBE_OFFSETS:
        magnitude = np.abs(np.fft.fftshift(probe_outputs[epsilon][0]))
        magnitude_db = 20.0 * np.log10(np.maximum(magnitude, 1e-6))

        spectrum_ax.plot(
            logical_bins,
            magnitude_db,
            ".-",
            label=f"epsilon = {epsilon:g}",
        )

    spectrum_ax.set_xlim(-12, 12)
    spectrum_ax.set_ylim(-85, 5)
    spectrum_ax.set_xlabel("Logical FFT bin")
    spectrum_ax.set_ylabel("Magnitude (dB relative to unit amplitude)")
    spectrum_ax.set_title("Single transmitted carrier at bin +3")
    spectrum_ax.grid(True, alpha=0.3)
    spectrum_ax.legend()

    phase_ax = fig.add_subplot(grid[1, 3:])
    symbol_numbers = np.arange(PROBE_SYMBOLS)

    phase_ax.plot(
        symbol_numbers,
        np.degrees(predicted_phase),
        "-",
        label="Predicted",
    )
    phase_ax.plot(
        symbol_numbers,
        np.degrees(measured_phase),
        "o",
        label="Measured",
    )

    phase_ax.set_xlabel("OFDM symbol index")
    phase_ax.set_ylabel("Unwrapped phase (degrees)")
    phase_ax.set_title("Single-carrier phase progression: epsilon = 0.25")
    phase_ax.grid(True, alpha=0.3)
    phase_ax.legend()

    fig.suptitle(
        "OFDM frequency offset: rotation, leakage, and correction\n"
        "Constellations at epsilon = 0.25 (250 Hz)"
    )

    fig.savefig(OUTPUT_PATH, dpi=160)
    print(f"Saved plot to: {OUTPUT_PATH}")
    plt.show()


if __name__ == "__main__":
    main()

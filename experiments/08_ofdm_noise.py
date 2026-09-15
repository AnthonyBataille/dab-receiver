from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from numpy.typing import NDArray

FFT_SIZE = 64
CP_LENGTH = 16
NUM_SYMBOLS = 5_000
SAMPLE_RATE = 64_000.0

ACTIVE_CARRIERS = np.concatenate(
    (
        np.arange(-8, 0),
        np.arange(1, 9),
    )
)
ACTIVE_BINS = ACTIVE_CARRIERS % FFT_SIZE
NUM_ACTIVE = len(ACTIVE_BINS)

SNR_VALUES_DB = (0.0, 10.0, 20.0)
STATISTICAL_TOLERANCE = 0.05

DATA_RNG = np.random.default_rng(2026)
NOISE_RNG = np.random.default_rng(2027)

OUTPUT_PATH = Path(__file__).resolve().with_suffix(".png")


def demodulate(received_blocks: NDArray[np.complex128]) -> NDArray[np.complex128]:
    """Recover active carriers using known, perfect symbol timing."""
    useful = received_blocks[:, CP_LENGTH : CP_LENGTH + FFT_SIZE]
    spectrum = np.fft.fft(useful, axis=1)
    return spectrum[:, ACTIVE_BINS]


def calculate_evm(
    received_symbols: NDArray[np.complex128], reference_symbols: NDArray[np.complex128]
) -> float:
    """Return RMS EVM as a ratio, not a percentage."""
    error_power = np.mean(np.abs(received_symbols - reference_symbols) ** 2)
    reference_power = np.mean(np.abs(reference_symbols) ** 2)
    return float(np.sqrt(error_power / reference_power))


def main() -> None:
    bits = DATA_RNG.integers(
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

    symbol_energy = float(np.mean(np.abs(tx_symbols) ** 2))
    signal_power = float(np.mean(np.abs(useful_samples) ** 2))

    expected_signal_power = NUM_ACTIVE * symbol_energy / FFT_SIZE**2

    assert np.isclose(
        signal_power,
        expected_signal_power,
        rtol=1e-12,
        atol=0.0,
    ), "Time-domain power does not match Parseval's identity."

    clean_symbols = demodulate(tx_blocks)
    round_trip_error = float(np.max(np.abs(clean_symbols - tx_symbols)))

    assert round_trip_error < 1e-12, "The noiseless OFDM round trip failed."

    print(f"FFT size: {FFT_SIZE}")
    print(f"Active carriers: {NUM_ACTIVE}")
    print(f"OFDM symbols: {NUM_SYMBOLS}")
    print(f"CP length: {CP_LENGTH} samples")
    print(f"Sample rate: {SAMPLE_RATE / 1_000:.1f} ksample/s")
    print(f"QPSK symbol energy: {symbol_energy:.8f}")
    print(f"Useful-sample power: {signal_power:.8f}")
    print(f"Ideal round-trip error: {round_trip_error:.3e}")

    results = []

    for snr_db in SNR_VALUES_DB:
        snr_linear = 10.0 ** (snr_db / 10.0)
        target_noise_power = signal_power / snr_linear

        noise = np.sqrt(target_noise_power / 2.0) * (
            NOISE_RNG.standard_normal(tx_blocks.shape)
            + 1j * NOISE_RNG.standard_normal(tx_blocks.shape)
        )

        received_blocks = tx_blocks + noise
        received_symbols = demodulate(received_blocks)

        useful_noise = noise[:, CP_LENGTH : CP_LENGTH + FFT_SIZE]
        measured_noise_power = float(np.mean(np.abs(useful_noise) ** 2))

        measured_snr_db = float(10.0 * np.log10(signal_power / measured_noise_power))

        carrier_snr_linear = symbol_energy / (FFT_SIZE * target_noise_power)
        carrier_snr_db = float(10.0 * np.log10(carrier_snr_linear))

        theoretical_evm = float(1.0 / np.sqrt(carrier_snr_linear))
        measured_evm = calculate_evm(
            received_symbols,
            tx_symbols,
        )

        detected_bits = np.stack(
            (
                received_symbols.real < 0.0,
                received_symbols.imag < 0.0,
            ),
            axis=-1,
        )

        bit_errors = detected_bits != bits
        symbol_errors = np.any(bit_errors, axis=-1)

        bit_error_rate = float(np.mean(bit_errors))
        symbol_error_rate = float(np.mean(symbol_errors))
        symbol_error_count = int(np.count_nonzero(symbol_errors))

        results.append(
            {
                "snr_db": snr_db,
                "measured_snr_db": measured_snr_db,
                "carrier_snr_db": carrier_snr_db,
                "target_noise_power": target_noise_power,
                "measured_noise_power": measured_noise_power,
                "theoretical_evm": theoretical_evm,
                "measured_evm": measured_evm,
                "bit_error_rate": bit_error_rate,
                "symbol_error_rate": symbol_error_rate,
                "symbol_error_count": symbol_error_count,
                "received_symbols": received_symbols,
            }
        )

    print()
    print(
        "Target SNR  Meas. SNR  Carrier SNR  "
        "EVM meas.  EVM theory   SER       BER     Symbol errors"
    )
    print("-" * 104)

    for result in results:
        print(
            f"{result['snr_db']:9.1f} "
            f"{result['measured_snr_db']:10.3f} "
            f"{result['carrier_snr_db']:12.3f} "
            f"{100 * result['measured_evm']:9.3f}% "
            f"{100 * result['theoretical_evm']:10.3f}% "
            f"{result['symbol_error_rate']:9.3e} "
            f"{result['bit_error_rate']:9.3e} "
            f"{result['symbol_error_count']:8d}"
        )

        power_ratio = result["measured_noise_power"] / result["target_noise_power"]
        evm_ratio = result["measured_evm"] / result["theoretical_evm"]

        assert abs(power_ratio - 1.0) < STATISTICAL_TOLERANCE, (
            f"Noise power mismatch at {result['snr_db']} dB: "
            f"measured/target = {power_ratio:.6f}"
        )
        assert abs(evm_ratio - 1.0) < STATISTICAL_TOLERANCE, (
            f"EVM mismatch at {result['snr_db']} dB: "
            f"measured/theory = {evm_ratio:.6f}"
        )

    print()
    print("Noise-power verification:")

    for result in results:
        print(
            f"  {result['snr_db']:4.0f} dB: "
            f"target = {result['target_noise_power']:.6e}, "
            f"measured = {result['measured_noise_power']:.6e}"
        )

    print()
    print("EVM reduction between successive SNR values:")

    for lower, higher in zip(results[:-1], results[1:]):
        snr_increase_db = higher["snr_db"] - lower["snr_db"]
        expected_reduction = 10.0 ** (snr_increase_db / 20.0)

        measured_reduction = lower["measured_evm"] / higher["measured_evm"]

        print(
            f"  {lower['snr_db']:.0f} -> {higher['snr_db']:.0f} dB: "
            f"measured = {measured_reduction:.4f}, "
            f"expected = {expected_reduction:.4f}"
        )

        assert (
            abs(measured_reduction / expected_reduction - 1.0) < STATISTICAL_TOLERANCE
        ), "EVM reduction does not match the SNR increase."

    print()
    print("All AWGN checks passed.")

    fig = plt.figure(figsize=(13, 8), constrained_layout=True)
    grid = fig.add_gridspec(2, 3)

    ideal_points = np.array(
        [
            1 + 1j,
            -1 + 1j,
            -1 - 1j,
            1 - 1j,
        ]
    ) / np.sqrt(2.0)

    points_to_plot = 6_000

    for column, result in enumerate(results):
        ax = fig.add_subplot(grid[0, column])

        points = result["received_symbols"].ravel()[:points_to_plot]

        ax.scatter(
            points.real,
            points.imag,
            s=5,
            alpha=0.20,
            color="tab:blue",
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
        ax.set_title(
            f"Sample SNR: {result['snr_db']:.0f} dB\n"
            f"EVM: {100 * result['measured_evm']:.2f}%"
        )
        ax.grid(True, alpha=0.3)

        if column == 0:
            ax.legend(loc="upper right")

    evm_ax = fig.add_subplot(grid[1, :])

    snr_curve_db = np.linspace(
        min(SNR_VALUES_DB),
        max(SNR_VALUES_DB),
        200,
    )
    theory_curve_percent = (
        100.0 * np.sqrt(NUM_ACTIVE / FFT_SIZE) * 10.0 ** (-snr_curve_db / 20.0)
    )

    evm_ax.semilogy(
        snr_curve_db,
        theory_curve_percent,
        color="tab:orange",
        label="Theoretical active-carrier EVM",
    )
    evm_ax.semilogy(
        [result["snr_db"] for result in results],
        [100 * result["measured_evm"] for result in results],
        "o",
        color="tab:blue",
        label="Measured EVM",
    )

    evm_ax.set_xlabel("Useful-sample SNR (dB)")
    evm_ax.set_ylabel("RMS EVM (%)")
    evm_ax.set_title("OFDM constellation error versus sample SNR")
    evm_ax.grid(True, which="both", alpha=0.3)
    evm_ax.legend()

    fig.suptitle("OFDM with additive white Gaussian noise")
    fig.savefig(OUTPUT_PATH, dpi=160)

    print(f"Saved plot to: {OUTPUT_PATH}")

    plt.show()


if __name__ == "__main__":
    main()

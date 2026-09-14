from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

FFT_SIZE = 64
SAMPLE_RATE = 64_000.0
NUM_SYMBOLS = 200

ECHO_DELAY = 8
ECHO_GAIN = 0.5

PREFIX_LENGTHS = (0, 4, 8, 16)
TOLERANCE = 1e-12


def receive_with_prefix(prefix_length: int, x: np.ndarray, h: np.ndarray):
    if not 0 <= prefix_length <= FFT_SIZE:
        raise ValueError("Prefix length must be between 0 and FFT_SIZE.")

    if prefix_length == 0:
        tx_blocks = x.copy()
    else:
        prefixes = x[:, -prefix_length:]
        tx_blocks = np.concatenate((prefixes, x), axis=1)

    tx_stream = tx_blocks.reshape(-1)

    # Physical channel: ordinary linear convolution of the entire stream.
    rx_stream = np.convolve(tx_stream, h, mode="full")

    block_length = FFT_SIZE + prefix_length

    # The extra convolution tail follows the final transmitted block.
    # Our FFT windows are all inside the first len(tx_stream) samples.
    rx_blocks = rx_stream[: tx_stream.size].reshape(NUM_SYMBOLS, block_length)

    useful_samples = rx_blocks[:, prefix_length : prefix_length + FFT_SIZE]

    Y = np.fft.fft(useful_samples, axis=1)

    return Y


def main():
    rng = np.random.default_rng(7)

    logical_carriers = np.concatenate(
        (
            np.arange(-8, 0),
            np.arange(1, 9),
        )
    )
    active_bins = logical_carriers % FFT_SIZE
    num_active = len(active_bins)

    print(f"FFT size: {FFT_SIZE}")
    print(f"Active carriers: {num_active}")
    print(f"OFDM symbols: {NUM_SYMBOLS}")
    print(f"Sample rate: {SAMPLE_RATE / 1e3:.1f} ksample/s")
    print(f"Carrier spacing: {SAMPLE_RATE / FFT_SIZE:.1f} Hz")
    print(
        f"Echo delay: {ECHO_DELAY} samples "
        f"= {1e6 * ECHO_DELAY / SAMPLE_RATE:.1f} us"
    )
    print(f"Echo amplitude: {ECHO_GAIN}")

    bits = rng.integers(
        0,
        2,
        size=(NUM_SYMBOLS, num_active, 2),
    )

    qpsk = ((1 - 2 * bits[:, :, 0]) + 1j * (1 - 2 * bits[:, :, 1])) / np.sqrt(2.0)

    X = np.zeros(
        (NUM_SYMBOLS, FFT_SIZE),
        dtype=np.complex128,
    )
    X[:, active_bins] = qpsk

    x = np.fft.ifft(X, axis=1)

    round_trip = np.fft.fft(x, axis=1)
    round_trip_error = np.max(np.abs(round_trip - X))

    assert round_trip_error < TOLERANCE

    print(f"\nIdeal FFT/IFFT round-trip error: " f"{round_trip_error:.3e}")

    h = np.zeros(ECHO_DELAY + 1, dtype=np.complex128)
    h[0] = 1.0
    h[ECHO_DELAY] = ECHO_GAIN

    H = np.fft.fft(h, n=FFT_SIZE)

    min_channel_magnitude = np.min(np.abs(H[active_bins]))

    assert min_channel_magnitude > 0.1

    print(f"Minimum |H[k]| on active carriers: " f"{min_channel_magnitude:.6f}")

    results = {}

    # Exclude the first symbol from measurements because it has no preceding
    # transmitted symbol. All remaining symbols have a realistic predecessor.
    reference = X[1:]
    reference_active = reference[:, active_bins]

    print("\n" " CP   Prefix time   Max |Y-HX|    Equalized EVM")
    print("------------------------------------------------")

    for prefix_length in PREFIX_LENGTHS:
        Y = receive_with_prefix(prefix_length, x, h)
        measured = Y[1:]

        expected = reference * H[np.newaxis, :]
        model_error = np.max(np.abs(measured - expected))

        equalized = measured[:, active_bins] / H[active_bins]

        error_power = np.mean(np.abs(equalized - reference_active) ** 2)
        reference_power = np.mean(np.abs(reference_active) ** 2)
        evm = np.sqrt(error_power / reference_power)

        results[prefix_length] = {
            "received": measured,
            "equalized": equalized,
            "model_error": model_error,
            "evm": evm,
        }

        prefix_time_us = 1e6 * prefix_length / SAMPLE_RATE

        print(
            f"{prefix_length:3d}"
            f"   {prefix_time_us:8.1f} us"
            f"   {model_error:11.3e}"
            f"   {100 * evm:12.6f}%"
        )

        if prefix_length >= ECHO_DELAY:
            assert (
                model_error < TOLERANCE
            ), "Sufficient prefix failed the circular-convolution check."
            assert evm < TOLERANCE, "Equalization failed with a sufficient prefix."
        else:
            assert (
                model_error > 1e-3
            ), "Expected a visible model error with an insufficient prefix."
            assert evm > 1e-3, "Expected residual distortion after equalization."

    print("\nAll cyclic-prefix checks passed.")

    ideal_points = np.array(
        [
            1 + 1j,
            1 - 1j,
            -1 + 1j,
            -1 - 1j,
        ]
    ) / np.sqrt(2.0)

    plot_prefixes = (0, 4, 16)
    symbols_to_plot = 30

    fig, axes = plt.subplots(
        2,
        3,
        figsize=(12, 8),
        sharex=True,
        sharey=True,
        constrained_layout=True,
    )

    for column, prefix_length in enumerate(plot_prefixes):
        result = results[prefix_length]

        received_points = result["received"][:symbols_to_plot, active_bins].reshape(-1)

        equalized_points = result["equalized"][:symbols_to_plot].reshape(-1)

        for row, points in enumerate(
            (
                received_points,
                equalized_points,
            )
        ):
            ax = axes[row, column]

            ax.scatter(
                points.real,
                points.imag,
                s=12,
                alpha=0.45,
                label="Measured",
            )
            ax.scatter(
                ideal_points.real,
                ideal_points.imag,
                marker="x",
                color="red",
                s=90,
                linewidths=2,
                label="Transmitted QPSK",
            )

            ax.axhline(0, color="gray", linewidth=0.6)
            ax.axvline(0, color="gray", linewidth=0.6)
            ax.grid(True, alpha=0.3)
            ax.set_aspect("equal", adjustable="box")
            ax.set_xlim(-2, 2)
            ax.set_ylim(-2, 2)
            ax.set_xlabel("In-phase")
            ax.set_ylabel("Quadrature")

        axes[0, column].set_title(f"CP = {prefix_length}: before equalization")
        axes[1, column].set_title(
            f"After equalization\n" f"EVM = {100 * result['evm']:.3f}%"
        )

    axes[0, 0].legend(loc="upper left", fontsize=8)

    fig.suptitle(
        "OFDM through a direct path and an 8-sample echo",
        fontsize=14,
    )

    output_path = Path(__file__).resolve().with_name("07_ofdm_cyclic_prefix.png")
    fig.savefig(output_path, dpi=160)

    print(f"Saved plot to: {output_path}")

    plt.show()


if __name__ == "__main__":
    main()

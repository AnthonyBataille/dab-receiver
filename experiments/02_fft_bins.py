from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.axes import Axes
import numpy as np


SAMPLE_RATE_HZ = 8000.0
NUM_SAMPLES = 8


def complex_tone(frequency_hz: float) -> np.ndarray:
    n = np.arange(NUM_SAMPLES)
    return np.exp(
        1j * 2 * np.pi * frequency_hz * n / SAMPLE_RATE_HZ
    )


def real_cosine(frequency_hz: float) -> np.ndarray:
    n = np.arange(NUM_SAMPLES)
    return np.cos(
        2 * np.pi * frequency_hz * n / SAMPLE_RATE_HZ
    )


def normalized_centered_spectrum(
    signal: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    spectrum = np.fft.fft(signal) / NUM_SAMPLES
    frequencies = np.fft.fftfreq(
        NUM_SAMPLES,
        d=1.0 / SAMPLE_RATE_HZ,
    )

    return (
        np.fft.fftshift(frequencies),
        np.abs(np.fft.fftshift(spectrum)),
    )


def plot_spectrum(
    axis: Axes,
    signal: np.ndarray,
    title: str,
) -> None:
    frequencies, magnitudes = normalized_centered_spectrum(signal)

    axis.stem(frequencies / 1000, magnitudes, basefmt=" ")
    axis.set_title(title)
    axis.set_xlabel("Frequency (kHz)")
    axis.set_ylabel("Normalized magnitude")
    axis.set_xticks(frequencies / 1000)
    axis.set_ylim(0, 1.1)
    axis.grid()


def main() -> None:
    positive_tone = complex_tone(2000)
    negative_tone = complex_tone(-1000)
    cosine = real_cosine(2000)
    off_bin_tone = complex_tone(2500)

    # Verify the unshifted, unnormalized DFT coefficients.
    positive_fft = np.fft.fft(positive_tone)
    negative_fft = np.fft.fft(negative_tone)
    cosine_fft = np.fft.fft(cosine)

    np.testing.assert_allclose(
        np.abs(positive_fft[2]), 8.0, atol=1e-12
    )
    np.testing.assert_allclose(
        np.abs(negative_fft[7]), 8.0, atol=1e-12
    )
    np.testing.assert_allclose(
        np.abs(cosine_fft[[2, 6]]), [4.0, 4.0], atol=1e-12
    )

    print("All bin-centred FFT checks passed.")
    print(f"|FFT(+2 kHz)[2]| = {abs(positive_fft[2]):.6f}")
    print(f"|FFT(-1 kHz)[7]| = {abs(negative_fft[7]):.6f}")
    print(
        "|FFT(cosine)[2, 6]| = "
        f"{abs(cosine_fft[2]):.6f}, "
        f"{abs(cosine_fft[6]):.6f}"
    )

    fig, axes = plt.subplots(2, 2, figsize=(11, 7))

    plot_spectrum(axes[0, 0], positive_tone, "Complex tone: +2 kHz")
    plot_spectrum(axes[0, 1], negative_tone, "Complex tone: −1 kHz")
    plot_spectrum(axes[1, 0], cosine, "Real cosine: 2 kHz")
    plot_spectrum(axes[1, 1], off_bin_tone, "Complex tone: +2.5 kHz")

    fig.tight_layout()

    output_path = Path(__file__).resolve().parent / "02_fft_bins.png"
    fig.savefig(output_path, dpi=150)
    print(f"Saved plot to: {output_path.resolve()}")

    plt.show()


if __name__ == "__main__":
    main()
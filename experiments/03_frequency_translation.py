from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.axes import Axes
import numpy as np


SAMPLE_RATE_HZ = 2.048e6
NUM_SAMPLES = 4096

DESIRED_FREQUENCY_HZ = 300e3
SECOND_FREQUENCY_HZ = -500e3
MIXER_FREQUENCY_HZ = -300e3


def complex_tone(frequency_hz: float) -> np.ndarray:
    """Generate a unit-amplitude discrete complex exponential."""
    n = np.arange(NUM_SAMPLES)

    return np.exp(
        1j * 2 * np.pi * frequency_hz * n / SAMPLE_RATE_HZ
    )


def frequency_to_bin(frequency_hz: float) -> int:
    """Convert a bin-centred frequency to an unshifted DFT index."""
    signed_index = int(
        np.rint(frequency_hz * NUM_SAMPLES / SAMPLE_RATE_HZ)
    )

    return signed_index % NUM_SAMPLES


def normalized_spectrum(signal: np.ndarray) -> np.ndarray:
    """Compute the normalized, unshifted DFT."""
    return np.fft.fft(signal) / NUM_SAMPLES


def centered_spectrum(
    signal: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    """Return centered frequencies and normalized magnitudes."""
    spectrum = normalized_spectrum(signal)

    frequencies = np.fft.fftfreq(
        NUM_SAMPLES,
        d=1.0 / SAMPLE_RATE_HZ,
    )

    return (
        np.fft.fftshift(frequencies),
        np.abs(np.fft.fftshift(spectrum)),
    )


def magnitude_at(
    spectrum: np.ndarray,
    frequency_hz: float,
) -> float:
    """Read the magnitude of a bin-centred frequency component."""
    bin_index = frequency_to_bin(frequency_hz)
    return float(np.abs(spectrum[bin_index]))


def print_component(
    label: str,
    spectrum: np.ndarray,
    frequency_hz: float,
) -> None:
    magnitude = magnitude_at(spectrum, frequency_hz)

    print(
        f"{label}: "
        f"{frequency_hz / 1000:+.0f} kHz, "
        f"magnitude {magnitude:.6f}"
    )


def plot_spectrum(
    axis: Axes,
    signal: np.ndarray,
    title: str,
) -> None:
    frequencies, magnitudes = centered_spectrum(signal)

    axis.plot(frequencies / 1000, magnitudes)
    axis.set_title(title)
    axis.set_xlabel("Frequency (kHz)")
    axis.set_ylabel("Normalized magnitude")
    axis.set_xlim(-SAMPLE_RATE_HZ / 2000, SAMPLE_RATE_HZ / 2000)
    axis.set_ylim(0, 1.1)
    axis.grid()


def main() -> None:
    bin_spacing_hz = SAMPLE_RATE_HZ / NUM_SAMPLES

    print(f"FFT bin spacing: {bin_spacing_hz:.1f} Hz")

    desired_component = complex_tone(DESIRED_FREQUENCY_HZ)
    second_component = 0.5 * complex_tone(SECOND_FREQUENCY_HZ)

    input_signal = desired_component + second_component

    complex_mixer = complex_tone(MIXER_FREQUENCY_HZ)
    real_mixer = np.real(complex_tone(-MIXER_FREQUENCY_HZ))

    complex_mixed_signal = input_signal * complex_mixer
    real_mixed_signal = input_signal * real_mixer

    input_spectrum = normalized_spectrum(input_signal)
    complex_mixed_spectrum = normalized_spectrum(complex_mixed_signal)
    real_mixed_spectrum = normalized_spectrum(real_mixed_signal)

    # Verify the original spectrum.
    np.testing.assert_allclose(
        magnitude_at(input_spectrum, 300e3),
        1.0,
        atol=1e-12,
    )
    np.testing.assert_allclose(
        magnitude_at(input_spectrum, -500e3),
        0.5,
        atol=1e-12,
    )

    # Verify translation by the complex oscillator.
    np.testing.assert_allclose(
        magnitude_at(complex_mixed_spectrum, 0),
        1.0,
        atol=1e-12,
    )
    np.testing.assert_allclose(
        magnitude_at(complex_mixed_spectrum, -800e3),
        0.5,
        atol=1e-12,
    )

    # Verify the two copies created by the real oscillator.
    expected_real_components = {
        0: 0.5,
        600e3: 0.5,
        -800e3: 0.25,
        -200e3: 0.25,
    }

    for frequency_hz, expected_magnitude in expected_real_components.items():
        np.testing.assert_allclose(
            magnitude_at(real_mixed_spectrum, frequency_hz),
            expected_magnitude,
            atol=1e-12,
        )

    # A unit-magnitude complex mixer rotates samples without
    # changing their individual magnitudes.
    np.testing.assert_allclose(
        np.abs(complex_mixed_signal),
        np.abs(input_signal),
        atol=1e-12,
    )

    print("All frequency-translation checks passed.")

    print("\nInput components:")
    print_component("Input", input_spectrum, 300e3)
    print_component("Input", input_spectrum, -500e3)

    print("\nAfter complex mixing:")
    print_component("Output", complex_mixed_spectrum, 0)
    print_component("Output", complex_mixed_spectrum, -800e3)

    print("\nAfter real mixing:")
    for frequency_hz in expected_real_components:
        print_component(
            "Output",
            real_mixed_spectrum,
            frequency_hz,
        )

    fig, axes = plt.subplots(3, 1, figsize=(11, 9))

    plot_spectrum(
        axes[0],
        input_signal,
        "Original spectrum",
    )
    plot_spectrum(
        axes[1],
        complex_mixed_signal,
        "After mixing with a −300 kHz complex oscillator",
    )
    plot_spectrum(
        axes[2],
        real_mixed_signal,
        "After mixing with a 300 kHz real cosine",
    )

    fig.tight_layout()

    output_path = (
        Path(__file__).resolve().parent
        / "03_frequency_translation.png"
    )
    fig.savefig(output_path, dpi=150)

    print(f"\nSaved plot to: {output_path}")
    plt.show()


if __name__ == "__main__":
    main()
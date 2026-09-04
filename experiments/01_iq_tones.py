from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import numpy.typing as npt


SAMPLE_RATE_HZ = 2.048e6
TONE_FREQUENCY_HZ = 100e3
NUM_SAMPLES = 500
PLOT_SAMPLES = 20

ComplexArray = npt.NDArray[np.complex128]

def complex_tone(frequency_hz: float) -> ComplexArray:
    n = np.arange(NUM_SAMPLES)
    return np.exp(1j * 2 * np.pi * frequency_hz * n / SAMPLE_RATE_HZ,  dtype=np.complex128)


def main() -> None:
    positive_tone = complex_tone(TONE_FREQUENCY_HZ)
    negative_tone = complex_tone(-TONE_FREQUENCY_HZ)

    n = np.arange(PLOT_SAMPLES)
    time_us = n / SAMPLE_RATE_HZ * 1e6
    real_cosine = np.cos(
        2 * np.pi * TONE_FREQUENCY_HZ * n / SAMPLE_RATE_HZ
    )

    fig, axes = plt.subplots(2, 2, figsize=(11, 7))

    axes[0, 0].plot(time_us, real_cosine)
    axes[0, 0].set_title("Real cosine")
    axes[0, 0].set_xlabel("Time (µs)")
    axes[0, 0].set_ylabel("Amplitude")
    axes[0, 0].grid()

    axes[0, 1].plot(time_us, positive_tone.real[:PLOT_SAMPLES], label="I")
    axes[0, 1].plot(time_us, positive_tone.imag[:PLOT_SAMPLES], label="Q")
    axes[0, 1].set_title("Complex IQ tone: +100 kHz")
    axes[0, 1].set_xlabel("Time (µs)")
    axes[0, 1].set_ylabel("Amplitude")
    axes[0, 1].legend()
    axes[0, 1].grid()

    axes[1, 0].plot(
        positive_tone.real[:PLOT_SAMPLES],
        positive_tone.imag[:PLOT_SAMPLES],
        marker=".",
    )
    axes[1, 0].set_title("Complex plane: +100 kHz")
    axes[1, 0].set_xlabel("I")
    axes[1, 0].set_ylabel("Q")
    axes[1, 0].set_aspect("equal")
    axes[1, 0].grid()

    axes[1, 1].plot(
        negative_tone.real[:PLOT_SAMPLES],
        negative_tone.imag[:PLOT_SAMPLES],
        marker=".",
    )
    axes[1, 1].set_title("Complex plane: −100 kHz")
    axes[1, 1].set_xlabel("I")
    axes[1, 1].set_ylabel("Q")
    axes[1, 1].set_aspect("equal")
    axes[1, 1].grid()

    fig.tight_layout()

    output_path = Path(__file__).resolve().parent / "01_iq_tones.png"
    fig.savefig(output_path, dpi=150)
    print(f"Saved plot to: {output_path.resolve()}")
    plt.show()


if __name__ == "__main__":
    main()
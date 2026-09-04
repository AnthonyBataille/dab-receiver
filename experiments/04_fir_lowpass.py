from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from numpy.typing import NDArray
from scipy import signal


SAMPLE_RATE_HZ = 2.048e6
NUM_SAMPLES = 32768

NUM_TAPS = 101
CUTOFF_HZ = 200e3

DESIRED_FREQUENCY_HZ = 100e3
UNWANTED_FREQUENCY_HZ = -800e3
UNWANTED_AMPLITUDE = 0.5


def complex_tone(
    frequency_hz: float,
    sample_indices: np.ndarray,
) -> np.ndarray:
    return np.exp(
        1j
        * 2
        * np.pi
        * frequency_hz
        * sample_indices
        / SAMPLE_RATE_HZ
    )


def make_input_signal(sample_indices: np.ndarray) -> np.ndarray:
    desired = complex_tone(
        DESIRED_FREQUENCY_HZ,
        sample_indices,
    )

    unwanted = UNWANTED_AMPLITUDE * complex_tone(
        UNWANTED_FREQUENCY_HZ,
        sample_indices,
    )

    return desired + unwanted


def response_at(
    taps: np.ndarray,
    frequency_hz: float,
) -> complex:
    """Evaluate H(exp(j omega)) directly at one frequency."""
    tap_indices = np.arange(len(taps))
    omega = 2 * np.pi * frequency_hz / SAMPLE_RATE_HZ

    return np.sum(taps * np.exp(-1j * omega * tap_indices))


def centered_spectrum_db(
    samples: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    spectrum = np.fft.fft(samples) / len(samples)
    spectrum = np.fft.fftshift(spectrum)

    frequencies = np.fft.fftfreq(
        len(samples),
        d=1.0 / SAMPLE_RATE_HZ,
    )
    frequencies = np.fft.fftshift(frequencies)

    magnitude = np.abs(spectrum)
    magnitude_db = 20 * np.log10(np.maximum(magnitude, 1e-14))

    return frequencies, magnitude_db


def main() -> None:
    taps: NDArray[np.float64] = np.asarray(
        signal.firwin(
            numtaps=NUM_TAPS,
            cutoff=CUTOFF_HZ,
            window="hamming",
            fs=SAMPLE_RATE_HZ,
            pass_zero=True,
        ),
        dtype=np.float64,
    )

    group_delay_samples = (NUM_TAPS - 1) // 2
    group_delay_seconds = group_delay_samples / SAMPLE_RATE_HZ

    # The coefficients of an odd-length linear-phase FIR filter
    # should be symmetric.
    np.testing.assert_allclose(
        taps,
        taps[::-1],
        atol=1e-15,
    )

    # firwin scales this low-pass filter to have unit DC gain.
    np.testing.assert_allclose(
        np.sum(taps),
        1.0,
        atol=1e-12,
    )

    desired_response = response_at(
        taps,
        DESIRED_FREQUENCY_HZ,
    )
    unwanted_response = response_at(
        taps,
        UNWANTED_FREQUENCY_HZ,
    )

    desired_gain = abs(desired_response)
    unwanted_gain = abs(unwanted_response)

    sample_indices = np.arange(NUM_SAMPLES)
    input_signal = make_input_signal(sample_indices)

    # Cold start: samples before n=0 are implicitly zero.
    cold_filtered = signal.lfilter(
        taps,
        [1.0],
        input_signal,
    )

    # Warm start: first supply the previous M-1 signal samples.
    previous_indices = np.arange(
        -(NUM_TAPS - 1),
        0,
    )
    extended_indices = np.concatenate(
        (previous_indices, sample_indices)
    )
    extended_input = make_input_signal(extended_indices)

    extended_filtered: NDArray[np.complex128] = np.asarray(
        signal.lfilter(
            taps,
            [1.0],
            extended_input,
        ),
        dtype=np.complex128,
    )

    warm_filtered = extended_filtered[NUM_TAPS - 1:]

    # Theoretical steady-state result obtained by applying H(f)
    # independently to each complex exponential.
    theoretical_output = (
        desired_response
        * complex_tone(
            DESIRED_FREQUENCY_HZ,
            sample_indices,
        )
        + UNWANTED_AMPLITUDE
        * unwanted_response
        * complex_tone(
            UNWANTED_FREQUENCY_HZ,
            sample_indices,
        )
    )

    # The warm output should be in steady state from n=0.
    np.testing.assert_allclose(
        warm_filtered,
        theoretical_output,
        atol=1e-11,
    )

    # The cold output reaches the same result once a complete
    # M-sample input history is available.
    np.testing.assert_allclose(
        cold_filtered[NUM_TAPS - 1:],
        theoretical_output[NUM_TAPS - 1:],
        atol=1e-11,
    )

    desired_gain_db = 20 * np.log10(desired_gain)
    unwanted_gain_db = 20 * np.log10(unwanted_gain)

    expected_phase = np.angle(
        np.exp(
            -1j
            * 2
            * np.pi
            * DESIRED_FREQUENCY_HZ
            * group_delay_samples
            / SAMPLE_RATE_HZ
        )
    )
    measured_phase = np.angle(desired_response)

    print(f"Number of taps: {NUM_TAPS}")
    print(
        "Group delay: "
        f"{group_delay_samples} samples = "
        f"{group_delay_seconds * 1e6:.6f} us"
    )
    print(f"Sum of coefficients: {np.sum(taps):.12f}")

    print("\nDesired component at +100 kHz:")
    print(f"  Filter magnitude: {desired_gain:.12f}")
    print(f"  Filter gain: {desired_gain_db:.3f} dB")
    print(f"  Measured phase: {measured_phase:.6f} rad")
    print(f"  Expected phase: {expected_phase:.6f} rad")

    print("\nUnwanted component at -800 kHz:")
    print(f"  Filter magnitude: {unwanted_gain:.12e}")
    print(f"  Filter gain: {unwanted_gain_db:.3f} dB")
    print(
        "  Output component magnitude: "
        f"{UNWANTED_AMPLITUDE * unwanted_gain:.12e}"
    )

    print("\nAll FIR filtering checks passed.")

    response_frequencies, frequency_response = signal.freqz(
        taps,
        worN=16384,
        fs=SAMPLE_RATE_HZ,
    )
    response_db = 20 * np.log10(
        np.maximum(np.abs(frequency_response), 1e-14)
    )

    input_frequencies, input_spectrum_db = centered_spectrum_db(
        input_signal
    )
    output_frequencies, output_spectrum_db = centered_spectrum_db(
        warm_filtered
    )

    fig, axes = plt.subplots(2, 2, figsize=(12, 8))

    axes[0, 0].stem(
        np.arange(NUM_TAPS),
        taps,
        basefmt=" ",
    )
    axes[0, 0].set_title("FIR impulse response")
    axes[0, 0].set_xlabel("Tap index")
    axes[0, 0].set_ylabel("Coefficient")
    axes[0, 0].grid()

    axes[0, 1].plot(
        response_frequencies / 1000,
        response_db,
    )
    axes[0, 1].axvline(
        CUTOFF_HZ / 1000,
        color="red",
        linestyle="--",
        label="Cutoff",
    )
    axes[0, 1].set_title("Filter magnitude response")
    axes[0, 1].set_xlabel("Frequency (kHz)")
    axes[0, 1].set_ylabel("Magnitude (dB)")
    axes[0, 1].set_ylim(-140, 5)
    axes[0, 1].legend()
    axes[0, 1].grid()

    axes[1, 0].plot(
        input_frequencies / 1000,
        input_spectrum_db,
        label="Input",
    )
    axes[1, 0].plot(
        output_frequencies / 1000,
        output_spectrum_db,
        label="Filtered output",
    )
    axes[1, 0].set_title("Input and steady-state output spectra")
    axes[1, 0].set_xlabel("Frequency (kHz)")
    axes[1, 0].set_ylabel("Normalized magnitude (dB)")
    axes[1, 0].set_xlim(
        -SAMPLE_RATE_HZ / 2000,
        SAMPLE_RATE_HZ / 2000,
    )
    axes[1, 0].set_ylim(-140, 5)
    axes[1, 0].legend()
    axes[1, 0].grid()

    transient_samples = 180

    axes[1, 1].plot(
        sample_indices[:transient_samples],
        np.abs(cold_filtered[:transient_samples]),
        label="Cold start",
    )
    axes[1, 1].plot(
        sample_indices[:transient_samples],
        np.abs(warm_filtered[:transient_samples]),
        label="Warm / steady state",
    )
    axes[1, 1].axvline(
        NUM_TAPS - 1,
        color="red",
        linestyle="--",
        label="Complete input history",
    )
    axes[1, 1].set_title("Startup transient")
    axes[1, 1].set_xlabel("Sample index")
    axes[1, 1].set_ylabel("Output magnitude")
    axes[1, 1].legend()
    axes[1, 1].grid()

    fig.tight_layout()

    output_path = (
        Path(__file__).resolve().parent
        / "04_fir_lowpass.png"
    )
    fig.savefig(output_path, dpi=150)

    print(f"Saved plot to: {output_path}")
    plt.show()


if __name__ == "__main__":
    main()

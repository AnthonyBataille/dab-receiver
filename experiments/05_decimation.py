from collections.abc import Sequence
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from numpy.typing import NDArray
from scipy import signal

SAMPLE_RATE_HZ = 2_048_000.0

NUM_TAPS = 101
CUTOFF_HZ = 200e3

DESIRED_FREQUENCY_HZ = 100_000.0
DESIRED_AMPLITUDE = 1.0
UNWANTED_FREQUENCY_HZ = -800_000.0
UNWANTED_AMPLITUDE = 0.5

DURATION = 0.010
DECIMATION_FACTOR = 4


def complex_tone(
    frequency_hz: float,
    sample_indices: NDArray[np.int64],
    sample_rate: float,
) -> NDArray[np.complex128]:
    return np.exp(1j * 2 * np.pi * frequency_hz * sample_indices / sample_rate)


def alias_frequency(
    frequency: float,
    sample_rate: float,
) -> float:
    return (frequency + sample_rate / 2.0) % sample_rate - sample_rate / 2.0


def make_input_signal() -> NDArray[np.complex128]:
    num_samples = int(DURATION * SAMPLE_RATE_HZ)
    sample_indices = np.arange(num_samples)
    input_signal = DESIRED_AMPLITUDE * complex_tone(
        DESIRED_FREQUENCY_HZ, sample_indices, SAMPLE_RATE_HZ
    ) + UNWANTED_AMPLITUDE * complex_tone(
        UNWANTED_FREQUENCY_HZ, sample_indices, SAMPLE_RATE_HZ
    )
    return input_signal


def num_settling_output_samples() -> int:
    return (NUM_TAPS - 1 + DECIMATION_FACTOR - 1) // DECIMATION_FACTOR


def measure_tones(
    signal: NDArray[np.complex128],
    frequencies: Sequence[float],
    sample_rate: float,
) -> NDArray[np.complex128]:
    indices = np.arange(len(signal))

    basis = np.column_stack(
        [
            np.exp(1j * 2.0 * np.pi * frequency * indices / sample_rate)
            for frequency in frequencies
        ]
    )

    coefficients, _, _, _ = np.linalg.lstsq(basis, signal, rcond=None)

    return coefficients


def filter_response_at(
    taps: NDArray[np.float64],
    frequency: float,
    sample_rate: float,
) -> np.complex128:
    indices = np.arange(len(taps))

    return np.complex128(
        np.sum(taps * np.exp(-1j * 2.0 * np.pi * frequency * indices / sample_rate))
    )


def calculate_spectrum(
    signal: NDArray[np.complex128], sample_rate: float
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    window = np.hanning(len(signal))
    coherent_gain = np.sum(window)

    spectrum = np.fft.fftshift(np.fft.fft(signal * window))

    frequencies = np.fft.fftshift(np.fft.fftfreq(len(signal), d=1.0 / sample_rate))

    magnitude = np.abs(spectrum) / coherent_gain
    magnitude_db = 20.0 * np.log10(np.maximum(magnitude, 1e-12))

    return frequencies, magnitude_db


def main() -> None:
    print(f"Input sample rate: {SAMPLE_RATE_HZ} Hz")
    sample_count = int(SAMPLE_RATE_HZ * DURATION)
    output_sample_rate = SAMPLE_RATE_HZ / DECIMATION_FACTOR
    print(f"Output sample rate: {output_sample_rate} Hz")
    print(f"Decimation factor: {DECIMATION_FACTOR}")

    assert output_sample_rate == 512_000.0
    assert sample_count % DECIMATION_FACTOR == 0

    input_signal = make_input_signal()
    direct_output = input_signal[::DECIMATION_FACTOR]
    print(f"Input samples: {sample_count}")
    print(f"Output samples: {len(direct_output)}")

    assert len(direct_output) == len(input_signal) // DECIMATION_FACTOR

    settling_output_samples = num_settling_output_samples()
    direct_output_steady = direct_output[settling_output_samples:]

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
    filtered_signal = np.asarray(
        signal.lfilter(
            taps,
            [1.0],
            input_signal,
        ),
        dtype=np.complex128,
    )

    filtered_output = filtered_signal[::DECIMATION_FACTOR]

    input_duration = len(input_signal) / SAMPLE_RATE_HZ
    direct_output_duration = len(direct_output) / output_sample_rate
    filtered_output_duration = len(filtered_output) / output_sample_rate

    assert np.isclose(input_duration, direct_output_duration)
    assert np.isclose(input_duration, filtered_output_duration)

    print(f"Input duration: {DURATION * 1000} ms")
    print(f"Output duration: {direct_output_duration * 1000} ms")

    settling_output_samples = num_settling_output_samples()
    filtered_output_steady = filtered_output[settling_output_samples:]

    desired_output_frequency = alias_frequency(DESIRED_FREQUENCY_HZ, output_sample_rate)
    unwanted_output_frequency = alias_frequency(
        UNWANTED_FREQUENCY_HZ, output_sample_rate
    )
    assert np.isclose(desired_output_frequency, 100_000.0)
    assert np.isclose(unwanted_output_frequency, 224_000.0)
    output_frequencies = [
        desired_output_frequency,
        unwanted_output_frequency,
    ]
    print(f"Frequency mapping:")
    print(f"\t{DESIRED_FREQUENCY_HZ} Hz -> {desired_output_frequency}")
    print(f"\t{UNWANTED_FREQUENCY_HZ} Hz -> {unwanted_output_frequency}")

    direct_coefficients = measure_tones(
        direct_output_steady,
        output_frequencies,
        output_sample_rate,
    )

    filtered_coefficients = measure_tones(
        filtered_output_steady,
        output_frequencies,
        output_sample_rate,
    )

    direct_amplitudes = np.abs(direct_coefficients)
    filtered_amplitudes = np.abs(filtered_coefficients)

    print("Direct downsampling:")
    print(f"\tMeasured amplitude at +100 kHz: {direct_amplitudes[0]}")
    print(f"\tMeasured amplitude at +224 kHz: {direct_amplitudes[1]}")

    desired_filter_response = filter_response_at(
        taps, DESIRED_FREQUENCY_HZ, SAMPLE_RATE_HZ
    )
    unwanted_filter_response = filter_response_at(
        taps, UNWANTED_FREQUENCY_HZ, SAMPLE_RATE_HZ
    )

    expected_filtered_desired = DESIRED_AMPLITUDE * abs(desired_filter_response)
    expected_filtered_unwanted = UNWANTED_AMPLITUDE * abs(unwanted_filter_response)

    assert np.isclose(
        direct_amplitudes[0],
        DESIRED_AMPLITUDE,
        rtol=1e-10,
        atol=1e-10,
    )

    assert np.isclose(
        direct_amplitudes[1],
        UNWANTED_AMPLITUDE,
        rtol=1e-10,
        atol=1e-10,
    )

    assert np.isclose(
        filtered_amplitudes[0],
        expected_filtered_desired,
        rtol=1e-8,
        atol=1e-10,
    )

    assert np.isclose(
        filtered_amplitudes[1],
        expected_filtered_unwanted,
        rtol=1e-6,
        atol=1e-10,
    )

    print("Filter decimation:")
    print(f"\tMeasured amplitude at +100 kHz: {filtered_amplitudes[0]}")
    print(f"\tExpected amplitude from H(100 kHz): {expected_filtered_desired}")
    print(f"\tMeasured aliased amplitude at +224 kHz: {filtered_amplitudes[1]}")
    print(f"\tExpected amplitude from H(-800 kHz): {expected_filtered_unwanted}")
    print(
        f"\tAlias attenuation relative to direct downsampling: {20.0 * np.log10(np.max([1e-14, filtered_amplitudes[1] / direct_amplitudes[1]]))} dB"
    )

    fig = plt.figure(figsize=(12, 8), layout="constrained")
    grid = fig.add_gridspec(2, 2)

    input_ax = fig.add_subplot(grid[0, :])
    direct_ax = fig.add_subplot(grid[1, 0])
    filtered_ax = fig.add_subplot(grid[1, 1])

    frequencies, magnitude_db = calculate_spectrum(input_signal, SAMPLE_RATE_HZ)
    input_ax.set_ylim(-100, 5)
    input_ax.set_xlim(-1_024, 1_024)
    input_ax.plot(frequencies / 1e3, magnitude_db)
    input_ax.axvline(100.0, color="red", linestyle="--", label="desired")
    input_ax.axvline(-800.0, color="red", linestyle="--", label="unwanted")
    input_ax.set_title("Input spectrum")
    input_ax.set_xlabel("Frequency (kHz)")
    input_ax.set_ylabel("Magnitude (dB)")
    input_ax.grid()

    frequencies, magnitude_db = calculate_spectrum(
        direct_output_steady, output_sample_rate
    )
    direct_ax.set_ylim(-100, 5)
    direct_ax.set_xlim(-256, 256)
    direct_ax.plot(frequencies / 1e3, magnitude_db)
    direct_ax.axvline(100.0, color="red", linestyle="--", label="desired")
    direct_ax.axvline(224.0, color="red", linestyle="--", label="unwanted")
    direct_ax.set_title("Direct downsampling output spectrum")
    direct_ax.set_xlabel("Frequency (kHz)")
    direct_ax.set_ylabel("Magnitude (dB)")
    direct_ax.grid()

    frequencies, magnitude_db = calculate_spectrum(
        filtered_output_steady, output_sample_rate
    )
    filtered_ax.set_ylim(-100, 5)
    filtered_ax.set_xlim(-256, 256)
    filtered_ax.plot(frequencies / 1e3, magnitude_db)
    filtered_ax.axvline(100.0, color="red", linestyle="--", label="desired")
    filtered_ax.axvline(224.0, color="red", linestyle="--", label="unwanted")
    filtered_ax.set_title("Filtered decimated output spectrum")
    filtered_ax.set_xlabel("Frequency (kHz)")
    filtered_ax.set_ylabel("Magnitude (dB)")
    filtered_ax.grid()

    output_path = Path(__file__).resolve().parent / "05_decimation.png"
    fig.savefig(output_path, dpi=150)

    print(f"Saved plot to: {output_path}")
    plt.show()


if __name__ == "__main__":
    main()

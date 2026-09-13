from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from numpy.typing import NDArray

FFT_SIZE = 64
SAMPLE_RATE = 64_000.0

FIRST_ACTIVE_CARRIER = 1
LAST_ACTIVE_CARRIER = 8

RANDOM_SEED = 2026


def map_bits_to_qpsk(bits: np.ndarray) -> np.ndarray:
    assert bits.ndim == 1

    num_bits = len(bits)
    assert num_bits > 0 and num_bits % 2 == 0
    assert np.all((bits == 0) | (bits == 1))

    bit_pairs = bits.astype(np.float64).reshape((num_bits // 2, 2))
    symbols = ((1 - 2 * bit_pairs[:, 1]) + 1j * (1 - 2 * bit_pairs[:, 0])) / np.sqrt(
        2.0
    )

    return symbols


def test_map_bits_to_qpsk() -> None:
    all_bit_pairs = np.array(
        [
            0,
            0,
            0,
            1,
            1,
            1,
            1,
            0,
        ],
        dtype=np.uint8,
    )
    all_qpsk_symbols = map_bits_to_qpsk(all_bit_pairs)
    assert np.allclose(
        all_qpsk_symbols,
        np.array([1 + 1j, -1 + 1j, -1 - 1j, 1 - 1j]) / np.sqrt(2.0),
        rtol=1e-12,
    )


def demap_qpsk_to_bits(symbols: NDArray[np.complex128]) -> NDArray[np.uint8]:
    assert symbols.ndim == 1

    num_symbols = len(symbols)
    bit_pairs = np.stack(
        ((symbols.imag < 0.0).astype(np.uint8), (symbols.real < 0.0).astype(np.uint8)),
        axis=1,
    )
    return bit_pairs.reshape(2 * num_symbols)


def test_demap_qpsk_to_bits():
    canonical_bits = np.array(
        [
            0,
            0,
            0,
            1,
            1,
            1,
            1,
            0,
        ],
        dtype=np.uint8,
    )

    canonical_symbols = map_bits_to_qpsk(canonical_bits)
    canonical_recovered_bits = demap_qpsk_to_bits(canonical_symbols)

    assert np.array_equal(
        canonical_recovered_bits,
        canonical_bits,
    )


def compute_subcarrier_basis_matrix(
    logical_carriers: np.ndarray, num_samples: int
) -> np.ndarray:
    sample_indices = np.arange(num_samples)[:, np.newaxis]
    carrier_row = logical_carriers[np.newaxis, :]
    subcarrier_basis = np.matmul(sample_indices, carrier_row)
    subcarrier_basis = np.exp(1j * 2.0 * np.pi * subcarrier_basis / num_samples)
    return subcarrier_basis


def compute_gram_matrix(
    subcarrier_basis_matrix: np.ndarray, num_samples: int
) -> np.ndarray:
    active_carrier_count = subcarrier_basis_matrix.shape[1]
    gram_matrix = (
        np.matmul(subcarrier_basis_matrix.conj().T, subcarrier_basis_matrix)
        / num_samples
    )
    assert gram_matrix.shape == (
        active_carrier_count,
        active_carrier_count,
    )
    return gram_matrix


def compute_time_domain_ofdm_symbol(
    subcarrier_basis_matrix: np.ndarray, qpsk_symbols: np.ndarray, num_samples: int
) -> np.ndarray:
    manual_time_domain_symbol = (
        np.matmul(subcarrier_basis_matrix, qpsk_symbols) / num_samples
    )
    return manual_time_domain_symbol


def calculate_centered_spectrum(
    signal: np.ndarray,
    sample_rate: float,
    fft_size: int | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """Evaluate and center the spectrum of a finite signal.

    If fft_size is larger than len(signal), np.fft.fft implicitly
    appends zeros. This zero-padding evaluates the signal's DTFT on
    a denser frequency grid; it does not add information or improve
    the true frequency resolution.

    No window is applied here intentionally. The transmitted useful
    OFDM symbol is exactly the finite 64-sample block produced by the
    IFFT. Applying a Hann or another analysis window would modify
    those samples and would no longer display the spectrum of the
    waveform that we actually transmitted.
    """
    signal = np.asarray(signal)

    assert signal.ndim == 1
    assert len(signal) > 0
    assert sample_rate > 0.0

    if fft_size is None:
        fft_size = len(signal)

    if fft_size < len(signal):
        raise ValueError("fft_size must be at least as large as the signal.")

    spectrum = np.fft.fft(signal, n=fft_size)
    frequencies = np.fft.fftfreq(
        fft_size,
        d=1.0 / sample_rate,
    )

    return (
        np.fft.fftshift(frequencies),
        np.fft.fftshift(spectrum),
    )


def magnitude_to_db(
    values: np.ndarray,
    floor_db: float = -120.0,
) -> np.ndarray:
    """Convert complex values or magnitudes to decibels safely.

    The numerical floor avoids log10(0), which would otherwise
    produce negative infinity for theoretically zero FFT bins.
    """
    magnitudes = np.abs(values)

    magnitude_floor = 10.0 ** (floor_db / 20.0)
    safe_magnitudes = np.maximum(
        magnitudes,
        magnitude_floor,
    )

    return 20.0 * np.log10(safe_magnitudes)


def plot_ofdm_experiment(
    frequency_domain_symbol: NDArray[np.complex128],
    time_domain_symbol: NDArray[np.complex128],
    transmitted_qpsk_symbols: NDArray[np.complex128],
    recovered_qpsk_symbols: NDArray[np.complex128],
    logical_carriers: NDArray,
    sample_rate: float,
    output_path: Path,
) -> None:
    """Create the four diagnostic plots for the OFDM round trip."""

    fft_size = len(frequency_domain_symbol)

    assert time_domain_symbol.shape == (fft_size,)
    assert transmitted_qpsk_symbols.shape == logical_carriers.shape
    assert recovered_qpsk_symbols.shape == logical_carriers.shape

    # This FFT is used only for the dense spectrum visualization.
    # The receiver itself still uses the original 64-point FFT.
    spectrum_fft_size = fft_size * 64

    figure, axes = plt.subplots(
        2,
        2,
        figsize=(14, 9),
    )

    # -------------------------------------------------------------
    # Plot 1: allocation at the actual 64 OFDM/FFT-bin frequencies
    # -------------------------------------------------------------
    allocation_frequencies = np.fft.fftshift(
        np.fft.fftfreq(
            fft_size,
            d=1.0 / sample_rate,
        )
    )

    centered_allocation = np.fft.fftshift(frequency_domain_symbol)

    allocation_axis = axes[0, 0]

    markerline, stemlines, baseline = allocation_axis.stem(
        allocation_frequencies / 1e3,
        np.abs(centered_allocation),
        basefmt=" ",
    )

    plt.setp(markerline, markersize=4)
    plt.setp(stemlines, linewidth=1.0)

    allocation_axis.set_title("Frequency-domain carrier allocation")
    allocation_axis.set_xlabel("Frequency (kHz)")
    allocation_axis.set_ylabel(r"$|X[k]|$")
    allocation_axis.set_xlim(
        -sample_rate / 2e3,
        sample_rate / 2e3,
    )
    allocation_axis.set_ylim(-0.05, 1.15)
    allocation_axis.grid(True, alpha=0.3)

    # -------------------------------------------------------------
    # Plot 2: the actual complex samples sent by the OFDM modulator
    # -------------------------------------------------------------
    sample_indices = np.arange(fft_size)
    time_microseconds = sample_indices / sample_rate * 1e6

    useful_symbol_duration_microseconds = fft_size / sample_rate * 1e6

    time_axis = axes[0, 1]

    time_axis.plot(
        time_microseconds,
        time_domain_symbol.real,
        marker=".",
        markersize=4,
        linewidth=1.0,
        label="I: real part",
    )

    time_axis.plot(
        time_microseconds,
        time_domain_symbol.imag,
        marker=".",
        markersize=4,
        linewidth=1.0,
        label="Q: imaginary part",
    )

    time_axis.set_title("Time-domain OFDM IQ samples")
    time_axis.set_xlabel("Time (µs)")
    time_axis.set_ylabel("Amplitude")
    time_axis.set_xlim(
        0.0,
        useful_symbol_duration_microseconds,
    )
    time_axis.grid(True, alpha=0.3)
    time_axis.legend()

    # -------------------------------------------------------------
    # Plot 3: transmitted and recovered active-carrier symbols
    # -------------------------------------------------------------
    constellation_axis = axes[1, 0]

    # Large unfilled circles allow the recovered crosses to remain
    # visible even though the two sets overlap almost perfectly.
    constellation_axis.scatter(
        transmitted_qpsk_symbols.real,
        transmitted_qpsk_symbols.imag,
        s=110,
        facecolors="none",
        edgecolors="tab:blue",
        linewidths=1.5,
        label="Transmitted",
    )

    constellation_axis.scatter(
        recovered_qpsk_symbols.real,
        recovered_qpsk_symbols.imag,
        s=45,
        marker="x",
        color="tab:red",
        linewidths=1.5,
        label="Recovered",
    )

    constellation_axis.axhline(
        0.0,
        color="black",
        linewidth=0.8,
        alpha=0.5,
    )
    constellation_axis.axvline(
        0.0,
        color="black",
        linewidth=0.8,
        alpha=0.5,
    )

    constellation_axis.set_title("QPSK constellation: ideal round trip")
    constellation_axis.set_xlabel("In-phase")
    constellation_axis.set_ylabel("Quadrature")
    constellation_axis.set_xlim(-1.1, 1.1)
    constellation_axis.set_ylim(-1.1, 1.1)
    constellation_axis.set_aspect(
        "equal",
        adjustable="box",
    )
    constellation_axis.grid(True, alpha=0.3)
    constellation_axis.legend()

    # -------------------------------------------------------------
    # Plot 4: densely evaluated finite-symbol spectrum
    # -------------------------------------------------------------
    dense_frequencies, dense_spectrum = calculate_centered_spectrum(
        time_domain_symbol,
        sample_rate,
        fft_size=spectrum_fft_size,
    )

    dense_spectrum_db = magnitude_to_db(
        dense_spectrum,
        floor_db=-120.0,
    )

    assert dense_frequencies.shape == (spectrum_fft_size,)
    assert dense_spectrum_db.shape == (spectrum_fft_size,)

    spectrum_axis = axes[1, 1]

    spectrum_axis.plot(
        dense_frequencies / 1e3,
        dense_spectrum_db,
        linewidth=1.0,
    )

    # Mark the active carrier centers. These are the frequencies at
    # which the 64-point receiver FFT evaluates the active symbols.
    for carrier in logical_carriers:
        carrier_frequency_khz = carrier * sample_rate / fft_size / 1e3

        spectrum_axis.axvline(
            carrier_frequency_khz,
            color="tab:red",
            linestyle=":",
            linewidth=0.6,
            alpha=0.35,
        )

    spectrum_axis.set_title("Spectrum between OFDM carrier centers")
    spectrum_axis.set_xlabel("Frequency (kHz)")
    spectrum_axis.set_ylabel("Magnitude (dB)")
    spectrum_axis.set_xlim(-20.0, 20.0)
    spectrum_axis.set_ylim(-80.0, 5.0)
    spectrum_axis.grid(True, alpha=0.3)

    figure.suptitle(
        "Synthetic OFDM modulator/demodulator",
        fontsize=14,
    )
    figure.tight_layout(rect=(0.0, 0.0, 1.0, 0.96))

    figure.savefig(
        output_path,
        dpi=160,
        bbox_inches="tight",
    )

    print(f"Saved plot to: {output_path}")

    plt.show()


def main() -> None:
    # Data and frequency-domain allocation
    useful_symbol_duration = FFT_SIZE / SAMPLE_RATE
    subcarrier_spacing = SAMPLE_RATE / FFT_SIZE
    assert np.isclose(useful_symbol_duration, 1e-3)
    assert np.isclose(subcarrier_spacing, 1_000.0)

    logical_carriers = np.concatenate((np.arange(-8, 0), np.arange(1, 9)))
    assert logical_carriers.shape == (16,)
    assert logical_carriers[0] == -8
    assert logical_carriers[7] == -1
    assert logical_carriers[8] == 1
    assert logical_carriers[-1] == 8
    assert 0 not in logical_carriers

    active_bins = logical_carriers % FFT_SIZE

    expected_active_bins = np.array(
        [
            56,
            57,
            58,
            59,
            60,
            61,
            62,
            63,
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
        ]
    )
    assert np.array_equal(active_bins, expected_active_bins)

    active_carrier_count = len(logical_carriers)
    bits_per_qpsk_symbol = 2
    num_transmitted_bits = active_carrier_count * bits_per_qpsk_symbol
    rng = np.random.default_rng(RANDOM_SEED)
    transmitted_bits = rng.integers(0, 2, size=num_transmitted_bits, dtype=np.uint8)
    # assert transmitted_bits.shape == (32,)
    assert transmitted_bits.dtype == np.uint8
    assert np.all((transmitted_bits == 0) | (transmitted_bits == 1))

    test_map_bits_to_qpsk()

    qpsk_symbols = map_bits_to_qpsk(transmitted_bits)
    assert qpsk_symbols.shape == (active_carrier_count,)
    assert np.iscomplexobj(qpsk_symbols)
    assert np.allclose(np.abs(qpsk_symbols), 1.0)

    frequency_domain_symbol = np.zeros(FFT_SIZE, dtype=np.complex128)
    frequency_domain_symbol[active_bins] = qpsk_symbols

    assert frequency_domain_symbol.shape == (FFT_SIZE,)
    assert np.iscomplexobj(frequency_domain_symbol)
    assert frequency_domain_symbol[0] == 0
    assert np.allclose(
        frequency_domain_symbol[active_bins],
        qpsk_symbols,
    )
    assert np.count_nonzero(frequency_domain_symbol) == active_carrier_count
    active_bins_bool_mask = np.zeros(FFT_SIZE, dtype=np.bool)
    active_bins_bool_mask[active_bins] = True
    assert active_bins_bool_mask[FFT_SIZE // 2] == False

    bit_pairs = transmitted_bits.reshape((-1, 2))
    print("Carrier allocation:\nlogical   bin   frequency   bits       QPSK")
    for index, active_carrier in enumerate(logical_carriers):
        active_bin = active_bins[index]
        print(
            f"{active_carrier:7d} "
            f"{active_bin:5d} "
            f"{active_carrier * subcarrier_spacing / 1000.0:9.1f} kHz "
            f"  [{bit_pairs[index][0]} {bit_pairs[index][1]}] "
            f"  {qpsk_symbols[index].real:.3f} + {qpsk_symbols[index].imag:.3f}j"
        )

    # Orthogonality and IFFT modulation
    print("OFDM parameters:")
    print(
        f"\tFFT size: {FFT_SIZE}\n"
        f"\tSample rate: {SAMPLE_RATE} Hz\n"
        f"\tUseful symbol duration: {useful_symbol_duration} s\n"
        f"\tSubcarrier spacing: {subcarrier_spacing} Hz\n"
        f"\tActive subcarriers: {active_carrier_count}"
    )
    subcarrier_basis_matrix = compute_subcarrier_basis_matrix(
        logical_carriers, FFT_SIZE
    )
    assert subcarrier_basis_matrix.shape == (
        FFT_SIZE,
        active_carrier_count,
    )
    assert np.iscomplexobj(subcarrier_basis_matrix)
    assert np.allclose(
        np.abs(subcarrier_basis_matrix),
        1.0,
    )
    gram_matrix = compute_gram_matrix(subcarrier_basis_matrix, FFT_SIZE)
    identity = np.eye(active_carrier_count)
    gram_error = gram_matrix - identity
    maximum_gram_error = np.max(np.abs(gram_error))
    assert np.allclose(
        gram_matrix,
        identity,
        rtol=0.0,
        atol=1e-12,
    )
    diagonal_bool_mask = np.eye(active_carrier_count, dtype=bool)
    maximum_gram_error_diagonal = np.max(np.abs(gram_error[diagonal_bool_mask]))
    maximum_gram_error_off_diagonal = np.max(np.abs(gram_error[~diagonal_bool_mask]))
    print("Orthogonality:")
    print(
        f"\tMaximum Gram-matrix error: {maximum_gram_error}\n"
        f"\tMaximum Gram-matrix error on diagonal: {maximum_gram_error_diagonal}\n"
        f"\tMaximum Gram-matrix error off diagonal: {maximum_gram_error_off_diagonal}\n"
    )
    manual_time_domain_symbol = compute_time_domain_ofdm_symbol(
        subcarrier_basis_matrix, qpsk_symbols, FFT_SIZE
    )
    assert manual_time_domain_symbol.shape == (FFT_SIZE,)
    assert np.iscomplexobj(manual_time_domain_symbol)
    time_domain_symbol = np.fft.ifft(frequency_domain_symbol)
    assert time_domain_symbol.shape == (FFT_SIZE,)
    assert np.iscomplexobj(time_domain_symbol)
    maximum_synthesis_error = np.max(
        np.abs(manual_time_domain_symbol - time_domain_symbol)
    )

    assert np.allclose(
        time_domain_symbol,
        manual_time_domain_symbol,
        rtol=0.0,
        atol=1e-12,
    )
    print("OFDM synthesis")
    print(
        f"\tMaximum error on time-domain synthetic OFDM symbol: {maximum_synthesis_error}"
    )

    frequency_domain_energy = np.sum(np.power(np.abs(frequency_domain_symbol), 2.0))
    time_domain_energy = np.sum(np.power(np.abs(time_domain_symbol), 2.0))
    average_time_domain_power = time_domain_energy / FFT_SIZE
    expected_time_domain_energy = 16.0 / FFT_SIZE
    expected_average_power = 16.0 / (FFT_SIZE**2)
    assert np.isclose(
        frequency_domain_energy,
        active_carrier_count,
    )
    assert np.isclose(
        time_domain_energy,
        expected_time_domain_energy,
    )
    assert np.isclose(
        average_time_domain_power,
        expected_average_power,
    )

    instantaneous_power = np.power(np.abs(time_domain_symbol), 2.0)
    peak_power = np.max(instantaneous_power)
    papr = peak_power / average_time_domain_power
    papr_db = 10.0 * np.log10(papr)
    assert papr >= 1.0
    assert papr_db >= 0.0
    print("Power")
    print(
        f"\tFrequency-domain energy: {frequency_domain_energy}\n"
        f"\tTime-domain energy: {time_domain_energy}\n"
        f"\tExpected time-domain energy: {expected_time_domain_energy}\n"
        f"\tAverage time-domain power: {average_time_domain_power}\n"
        f"\tExpected average power: {expected_average_power}\n"
        f"\tPAPR: {papr} ({papr_db} dB)"
    )

    # Receiver and QPSK demapper
    received_time_domain_symbol = time_domain_symbol.copy()
    assert np.array_equal(
        received_time_domain_symbol,
        time_domain_symbol,
    )
    recovered_frequency_domain_symbol = np.fft.fft(received_time_domain_symbol)
    frequency_domain_error_max = np.max(
        np.abs(recovered_frequency_domain_symbol - frequency_domain_symbol)
    )
    inactive_bin_leakage_max = np.max(
        np.abs(recovered_frequency_domain_symbol[~active_bins_bool_mask])
    )
    recovered_dc_bin = np.abs(recovered_frequency_domain_symbol[0])
    assert np.isclose(frequency_domain_error_max, 0.0, rtol=0.0, atol=1e-12)
    assert np.isclose(inactive_bin_leakage_max, 0.0, rtol=0.0, atol=1e-12)
    assert np.isclose(
        np.abs(recovered_frequency_domain_symbol[0]), 0.0, rtol=0.0, atol=1e-12
    )

    recovered_qpsk_symbols = recovered_frequency_domain_symbol[active_bins]
    qpsk_symbols_error_max = np.max(np.abs(recovered_qpsk_symbols - qpsk_symbols))
    assert np.allclose(
        recovered_qpsk_symbols,
        qpsk_symbols,
        rtol=0.0,
        atol=1e-12,
    )
    test_demap_qpsk_to_bits()

    recovered_bits = demap_qpsk_to_bits(recovered_qpsk_symbols)
    num_received_bits = len(recovered_bits)
    assert num_received_bits == num_transmitted_bits
    bit_error_count = np.count_nonzero(recovered_bits != transmitted_bits)
    ber = bit_error_count / num_received_bits
    assert np.array_equal(
        recovered_bits,
        transmitted_bits,
    )
    print("OFDM receiver")
    print(
        f"\tMaximum FFT/IFFT round-trip error: {frequency_domain_error_max}\n"
        f"\tMaximum active-carrier error: {qpsk_symbols_error_max}\n"
        f"\tMaximum inactive-carrier magnitude: {inactive_bin_leakage_max}\n"
        f"\tRecovered DC magnitude: {recovered_dc_bin}\n"
        f"\tTransmitted bits: {num_transmitted_bits}\n"
        f"\tBit errors: {bit_error_count}\n"
        f"\tBit-error rate: {ber}"
    )

    # Visualization
    plot_path = Path(__file__).resolve().parent / "06_ofdm_roundtrip.png"

    plot_ofdm_experiment(
        frequency_domain_symbol=frequency_domain_symbol,
        time_domain_symbol=time_domain_symbol,
        transmitted_qpsk_symbols=qpsk_symbols,
        recovered_qpsk_symbols=recovered_qpsk_symbols,
        logical_carriers=logical_carriers,
        sample_rate=SAMPLE_RATE,
        output_path=plot_path,
    )


if __name__ == "__main__":
    main()

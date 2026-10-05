# Offline IQ harness

The Phase 4 C++ harness reads a recording, reports its sample count and duration,
and can export power and spectrum CSVs. It is a starting point for offline DSP;
it does not synchronize or decode DAB, or acquire live samples.

## Input and reader

Input is a headerless file of unsigned 8-bit bytes in I, Q order. Each pair is
one complex sample; the sample rate is supplied separately in complex samples
per second. `IqFileReader` converts each component with `(byte - 128) / 128`
into `std::complex<float>` (I real, Q imaginary). For example, 0 maps to -1,
128 to 0, and 255 to 127/128.

`IqFileReader::read_next()` returns at most its configured block capacity
(default 4096 complex samples); an empty block means clean EOF. An unmatched
final I byte or an I/O error throws. Its optional raw read size supports tests
that split an I/Q pair across reads. The local, untracked reference recording
`dab_11d_10s_u8.iq` has 20,480,000 complex samples at 2,048,000 samples/s
(10 seconds).

## Run and plot

From the repository root in PowerShell, build and check the harness, then use
the local reference recording or substitute another raw U8 IQ file:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset test-debug

.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
.\.venv\Scripts\python.exe .\experiments\plot_iq_diagnostics.py `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv `
    --output-dir .\out
```

The CLI requires a positive integer sample rate. Without CSV options it prints
the input format, sample count, and duration. Either CSV option can be used
alone; each may appear once. The VS Code **Debug DAB receiver (summary)** and
**Debug DAB receiver (diagnostics)** configurations provide the same commands.
The Python script requires NumPy and Matplotlib; it reads the CSVs and writes
`power_time.png` and/or `spectrum.png` without processing IQ itself.

## Diagnostic conventions

| Output | Meaning |
| --- | --- |
| `power.csv`: `window_start_seconds,mean_linear_power,sample_count` | Nonoverlapping 512-sample windows, including a final partial window. Time is the first sample index divided by sample rate. Power is the mean of `I*I + Q*Q` in squared normalized sample units. Empty input produces only the header. |
| `spectrum.csv`: `relative_frequency_hz,linear_bin_power` | The first 2048 samples, rectangular window, forward transform with a negative exponent. FFT-shifted bin order is -1024 through +1023, spaced by sample rate / 2048 around the tuned center. Power is `|X[k]|^2 / 2048^2`, not calibrated RF power. |

The spectrum calculation is a bounded direct DFT for diagnostics, not the
planned production FFT. The plot displays spectrum power in dB relative to
unity with a -200 dB display floor. Summary, power, and spectrum collection
share one reader pass with bounded memory.

Malformed input, invalid arguments, and output errors return a nonzero exit
status. Spectrum export requires at least 2048 samples; summary and power work
with shorter files. Missing CSV parent directories are created, and repeat
runs replace existing CSVs after the full input and outputs have been checked.
On ordinary failure, previous CSVs are restored. An abrupt crash or external
file interference can leave temporary or backup files, so use the exit status
to determine success and avoid concurrent access to the destinations.

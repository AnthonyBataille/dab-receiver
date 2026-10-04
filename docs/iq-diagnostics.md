# Phase 4.4 offline diagnostics

From the repository root in Windows PowerShell:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset test-debug

# Original summary-only invocation remains supported.
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000

New-Item -ItemType Directory -Force .\out | Out-Null
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
.\.venv\Scripts\python.exe .\experiments\plot_iq_diagnostics.py `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv `
    --output-dir .\out
```

Before using Debug DAB receiver (diagnostics) in VS Code, create the out directory `(New-Item -ItemType Directory -Force .\out)`. On each later run, enter new names for both CSV files when prompted, such as `.\out\power-02.csv` and `.\out\spectrum-02.csv`. The exporter will not overwrite existing files. Pass those same names to the plotting script.

Either output option may be supplied alone, in either order, once each. The
sample rate must be a positive integer in complex samples per second. The plot
script uses the existing NumPy/Matplotlib environment and writes `power_time.png`
and/or `spectrum.png`. It reads CSV only; it does not decode IQ or calculate a DFT.
Empty power CSVs produce an empty plot. Spectrum display uses
`10*log10(max(linear_bin_power, 1e-20))`: a display-only floor of -200 dB relative
to unity, not calibrated dBm or a power spectral density.

## Numerical conventions

- Input: headerless unsigned 8-bit bytes in I, Q order. A complex sample is
  `I + jQ`; each component is converted with `(byte - 128) / 128`.
- Power CSV: `window_start_seconds,mean_linear_power,sample_count`. Instantaneous
  power is `I*I + Q*Q` in squared normalized sample units. Consecutive,
  nonoverlapping windows contain 512 complex samples, except the final partial
  window, which uses its actual count. The first sample has absolute index zero;
  each start time is `window_start_sample_index / sample_rate`, in seconds.
  There is no rounded-duration accumulation. Empty input emits just the header.
- Spectrum CSV: `relative_frequency_hz,linear_bin_power`. Only samples 0 through
  2047 are retained. A rectangular window (no taper) and forward transform
  `X[k] = sum(x[n] * exp(-j*2*pi*k*n/2048))` are used. Output is FFT-shifted:
  bins -1024 through +1023, at `bin * sample_rate / 2048` Hz relative to the
  tuned center. Spacing is `sample_rate / 2048` Hz; no absolute RF frequency is
  inferred. Linear bin power is `|X[k]|^2 / 2048^2`. A bin-centered complex tone
  of amplitude A has peak power A squared, including DC.
- The C++ direct DFT is a provisional, bounded diagnostic (2048 squared terms),
  **not the Phase 5 production FFT**. No FFT library is added. Summary, power,
  and collection of initial spectrum samples share one reader pass. Memory is
  bounded by a reader block, 2048 complex doubles, and constant accumulator state.
  Double-precision power accumulation always follows sample order, independent
  of reader block capacity. CSV numbers use the classic locale and 17 digits.

## Errors and publication

Invalid arguments, missing/unreadable or malformed IQ input, output open/write/
close errors, and fewer than 2048 samples when spectrum is requested cause a
nonzero exit with an error. Short files still support power and summary without
the spectrum option. A malformed trailing byte anywhere in the recording fails
the entire export, even after the initial spectrum samples were collected.

Output destinations must be **new files** in existing directories. Existing
files/directories (including the input) are never intentionally overwritten;
choose fresh CSV names for subsequent runs. Outputs are staged in uniquely
reserved sibling directories on the same volume. Both streams are flushed and
closed before renaming to their final paths. Ordinary exceptions remove staging
files and roll back any CSV already published in that invocation. Failed runs
therefore do not leave truncated CSVs with final names.

This is not a crash-atomic multi-file transaction: abrupt termination, external
file interference, or a filesystem refusing cleanup can leave a temporary
directory or an already complete CSV. Avoid concurrent writers to the same
destinations. The command's exit status determines success; summary text can
already have been printed before a final rename fails.

Generated diagnostics and plots belong under ignored `out/`. Recordings remain
outside Git. These diagnostics perform no null detection, synchronization,
service decoding, or live acquisition.

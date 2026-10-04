# Phase 4.4 offline diagnostics

From the repository root in Windows PowerShell:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset test-debug

# Original summary-only invocation remains supported.
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000

.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
.\.venv\Scripts\python.exe .\experiments\plot_iq_diagnostics.py `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv `
    --output-dir .\out
```

The VS Code **Debug DAB receiver (diagnostics)** configuration uses the same
default CSV paths. You can launch it from a fresh checkout or repeat it with the
same paths: the exporter creates `out/` and replaces completed CSVs after a
successful run. Use different filenames when you want to keep earlier results.

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

Missing output parent directories are created. Existing CSV files are replaced
only after the full input has been validated and both temporary CSV streams have
been flushed and closed. The input file and directory destinations are rejected.
Temporary files and backups are placed directly beside their destinations on
the same volume. Because Windows does not let `std::filesystem::rename` replace
an existing file, the exporter first renames the old CSV to a backup, then
renames the completed temporary CSV to the destination. It keeps both backups
until both CSVs are published. On an ordinary failure, it removes temporary
files and restores previous CSVs, including one already published in that run.

This is not a crash-atomic multi-file transaction: abrupt termination, external
file interference, or a filesystem refusing rollback or cleanup can leave a
temporary file, backup, or complete CSV. Replacement has a brief interval
between moving an old CSV aside and publishing its successor. Avoid concurrent
writers or readers that hold the output files open. The command's exit status
determines success; summary text can already have been printed before a final
rename fails.

Generated diagnostics and plots belong under ignored `out/`. Recordings remain
outside Git. These diagnostics perform no null detection, synchronization,
service decoding, or live acquisition.

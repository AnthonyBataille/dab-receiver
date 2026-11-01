# Offline IQ harness

## Purpose

The offline harness reads an IQ recording and reports its sample count,
duration, and detected null-symbol intervals. It can also export power and
spectrum CSVs for inspection.

It is a starting point for offline DSP. It does not decode DAB or acquire live
samples. See [Null-symbol detector](null-symbol-detector.md) for detection details.

## Usage

Build the project as described in [README](../README.md). From the repository
root in PowerShell, run a local recording with its sample rate:

```powershell
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000
```

To export and plot diagnostics:

```powershell
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
.\.venv\Scripts\python.exe .\experiments\plot_iq_diagnostics.py `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv `
    --output-dir .\out
```

Either CSV option can be used alone. Output directories are created as needed;
successful runs replace existing CSVs. Invalid input or output errors return a
nonzero exit status.

The plotting script requires NumPy and Matplotlib. It writes `power_time.png`
and/or `spectrum.png` from the CSVs.

## Input and outputs

Input is a headerless unsigned 8-bit file in I, Q order. Each byte pair is one
complex sample. Supply the sample rate as a positive integer in complex samples
per second. The local reference recording shown above is 10 seconds at
2,048,000 samples/s and is not stored in Git.

| Output | Meaning |
| --- | --- |
| Console summary | Sample count, duration, and any detected null-symbol intervals. |
| Power CSV | Mean `I*I + Q*Q` over each 512-sample window, with its start time and sample count. Includes a final partial window. |
| Spectrum CSV | Power per frequency bin from the first 2048 samples, centered on the tuned frequency. Requires at least 2048 samples. |

Power uses normalized sample units, not calibrated RF power. The spectrum plot
shows power in dB relative to unity.

## Implementation details

- **Reader:** `IqFileReader` converts bytes with `(byte - 128) / 128` into
  `std::complex<float>`, with I as real and Q as imaginary. It reads in blocks
  and rejects an unmatched final byte.
- **Runner:** `run_offline_iq` owns the processing loop and returns `IqSummary`.
  Optional `IqDiagnostics` collects power and spectrum data in the same pass.
- **Diagnostics:** power windows and spectrum collection are independent of
  reader block boundaries. The spectrum uses a direct DFT for diagnostics.
- **Output:** `iq_output` formats summaries and publishes CSVs. Existing CSVs
  are restored if publication fails during normal execution.

Source: `src/iq_file_reader.hpp/.cpp`, `src/offline_iq_runner.hpp/.cpp`,
`src/iq_diagnostics.hpp/.cpp`, and `src/iq_output.hpp/.cpp`.
Tests: `tests/iq_file_reader_test.cpp`, `tests/iq_diagnostics_test.cpp`, and
`tests/iq_cli_test.cmake`.

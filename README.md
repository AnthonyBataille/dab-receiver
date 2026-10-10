# DABReceiver

DABReceiver is a Windows application intended to receive and decode DAB+ broadcasts with an RTL-SDR Blog V3-compatible device.

The intended high-level receiver chain is:

`IQ input -> DAB ensemble synchronization -> service discovery -> DAB+ audio service selection -> playback`

- [Project scope](docs/project-scope.md)
- [Development environment](docs/development-environment.md)

Large IQ recordings are not stored in Git.

Offline IQ input, summaries, and power/spectrum diagnostics are documented in
[the offline IQ harness guide](docs/iq-offline-harness.md).

## Build and run

Configure with Visual Studio 2022 for x64:

```powershell
cmake --preset vs2022-x64
```

Build Debug:

```powershell
cmake --build --preset debug
```

Build Release:

```powershell
cmake --build --preset release
```

Run the Debug executable:

Display only file summary:
```powershell
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000
```

Display file summary and export diagnostic CSVs.
```powershell
.\build\Debug\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
```

Run the Release executable:

Display only file summary:
```powershell
.\build\Release\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000
```

Display file summary and export diagnostic CSVs.
```powershell
.\build\Release\dab_receiver.exe .\dab_11d_10s_u8.iq 2048000 `
    --power-csv .\out\power.csv --spectrum-csv .\out\spectrum.csv
```

## Test

Run the Debug test suite:

```powershell
ctest --preset test-debug
```

## Continuous integration

GitHub Actions runs the Windows Debug build and CTest on pushes to `main` and pull requests.

## Formatting

Format all tracked C++ source and header files:

```powershell
git ls-files -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' |
    ForEach-Object { clang-format -i --style=file $_ }
```

Check formatting without modifying files:

```powershell
git ls-files -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' |
    ForEach-Object { clang-format --dry-run --Werror --style=file $_ }
```

Licensing is deferred; no license is currently claimed.

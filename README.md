# DABReceiver

DABReceiver is a Windows application intended to receive and decode DAB+ broadcasts with an RTL-SDR Blog V3-compatible device.

The project is currently in Phase 0. The intended high-level receiver chain is:

`IQ input -> DAB ensemble synchronization -> service discovery -> DAB+ audio service selection -> playback`

- [Project scope](docs/project-scope.md)
- [Development environment](docs/development-environment.md)

Large IQ recordings are not stored in Git.

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

```powershell
.\build\Debug\dab_receiver.exe
```

Run the Release executable:

```powershell
.\build\Release\dab_receiver.exe
```

## Test

Run the Debug test suite:

```powershell
ctest --preset test-debug
```

Licensing is deferred; no license is currently claimed.

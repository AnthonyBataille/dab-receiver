# Development Environment

## Toolchain

- Windows 11 Home 25H2.
- Visual Studio Community 2022 17.14.37.
- MSVC 19.44.35228 for x64.
- CMake 4.4.0.
- Git 2.53.0.
- Python 3.12.1 virtual environment.
- pip 26.2.1.

## Clean-clone verification (2026-08-17)

- A fresh clone outside the existing working directory configured successfully with the `vs2022-x64` CMake preset.
- The Debug build and Debug CTest suite succeeded.
- `dab_receiver.exe` printed `DAB Receiver 0.1.0`.
- Catch2 was obtained and built by CMake FetchContent in the fresh build directory.

## SDR hardware and verified reference capture

- Device: RTL2832U with R820T tuner (RTL-SDR Blog V3).
- DAB block: 11D, center frequency 222.064 MHz.
- Sample rate: 2,048,000 complex samples/s.
- Format: raw interleaved unsigned 8-bit I, Q; one byte per component.
- Gain: automatic (no manual gain specified).
- Length: 20,480,000 complex samples = 10 seconds.
- Expected and observed size: 40,960,000 bytes.
- Reference recording: `dab_11d_10s_u8.iq` (stored outside Git).

The capture script is `.\experiments\capture-dab-11d.ps1`. Run it
from PowerShell, supplying the path to the installed executable and a new
output filename:

```powershell
.\experiments\capture-dab-11d.ps1 -RtlSdrExe "C:\path\to\rtl_sdr.exe" `
    -OutputFile "dab_11d_10s_u8.iq"
```

The `[R82XX] PLL not locked!` initialization warning was observed and retained for later investigation.

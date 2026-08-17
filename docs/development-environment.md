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

## SDR hardware and verified capture

- RTL2832U with R820T tuner.
- Verified 11D capture at 222.064 MHz.
- 2.048 MS/s, unsigned 8-bit interleaved IQ.
- Automatic gain.
- 20,480,000 complex samples and 40,960,000 bytes.

The `[R82XX] PLL not locked!` initialization warning was observed and retained for later investigation.

# DABReceiver Repository Instructions

This is a Windows C++20 DAB+ receiver project for RTL-SDR. Current work is Phase 0. Keep the GUI separate from the backend/DSP pipeline.

Before starting work, read `README.md`, this `AGENTS.md`, and the relevant files under `docs/`.

## Authoritative Commands

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
cmake --build --preset release
ctest --preset test-debug
```

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

Run C++ source formatting after edits. All created or modified text files must use CRLF line endings.

Never commit `build/`, `.venv/`, `rtl-sdr-64bit/`, `recordings/`, or IQ/raw captures. Do not commit or push unless explicitly asked.

Keep changes small and reviewable. Use objective validation before claiming implementation work complete.

- UNDERSTOOD = can explain and apply a concept.
- IMPLEMENTED = code exists and builds or runs.
- VERIFIED = objective output demonstrates correct behaviour.

Only VERIFIED work satisfies an implementation milestone.

Place authoritative sources beside DAB-standard constants, tables, bit orderings, and error-correction logic. Provide independent unit tests for pure transformations such as CRCs, scramblers, interleavers, puncturing maps, and bit packing.

Preserve intermediate DSP observability. Avoid optimization before a correct offline pipeline exists.

Do not make architectural or algorithmic changes without stating the intended change, evidence, risks, and validation plan.

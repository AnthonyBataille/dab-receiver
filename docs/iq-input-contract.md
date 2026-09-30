# Phase 4.1–4.2 IQ input contract

The offline input is a raw file with no header. Bytes are unsigned 8-bit values
alternating I, Q; each pair is one complex sample. The sample rate is supplied
separately and is not stored in the file. Converted samples use
`std::complex<float>` with I as the real component and Q as the imaginary
component.

The exercise normalization convention maps each unsigned byte `u` to
`(u - 128) / 128`: `0 → -1`, `128 → 0`, and `255 → 127/128`.

The local reference `dab_11d_10s_u8.iq` contains 20,480,000 complex samples
at 2.048 MS/s (10 seconds; 40,960,000 bytes). It is at the repository root
locally and is not tracked by Git.

`IqFileReader` returns at most its configured capacity in complex samples per
call. An empty block means clean EOF. Open/read errors and a truncated final
I/Q pair throw exceptions. The optional raw read byte count allows tests to
split a pair between reads.
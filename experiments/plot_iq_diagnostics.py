"""Plot C++ diagnostic CSVs only; no IQ processing or Fourier transform."""

import argparse
from pathlib import Path
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_csv(path, names):
    data = np.genfromtxt(path, delimiter=",", names=True, ndmin=1)
    if data.dtype.names != names:
        raise ValueError(f"{path}: expected columns {','.join(names)}")
    if any(not np.all(np.isfinite(data[name])) for name in names):
        raise ValueError(f"{path}: non-finite or malformed numeric data")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--power-csv", type=Path)
    parser.add_argument("--spectrum-csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("out"))
    args = parser.parse_args()
    if args.power_csv is None and args.spectrum_csv is None:
        parser.error("supply --power-csv and/or --spectrum-csv")
    try:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        if args.power_csv is not None:
            data = read_csv(args.power_csv,
                            ("window_start_seconds", "mean_linear_power", "sample_count"))
            if np.any(data["mean_linear_power"] < 0) or np.any(data["sample_count"] <= 0):
                raise ValueError("power CSV contains negative power or invalid sample counts")
            fig, ax = plt.subplots()
            ax.plot(data["window_start_seconds"], data["mean_linear_power"])
            ax.set(xlabel="Window start time (s)", ylabel="Mean linear power",
                   title="IQ power (512-sample windows; final window may be partial)")
            ax.grid(True)
            fig.savefig(args.output_dir / "power_time.png", dpi=150, bbox_inches="tight")
            plt.close(fig)
        if args.spectrum_csv is not None:
            data = read_csv(args.spectrum_csv, ("relative_frequency_hz", "linear_bin_power"))
            if len(data) != 2048 or np.any(data["linear_bin_power"] < 0):
                raise ValueError("spectrum CSV must contain 2048 bins with nonnegative power")
            # Display-only floor: 1e-20 linear power = -200 dB relative to unity.
            power_db = 10 * np.log10(np.maximum(data["linear_bin_power"], 1e-20))
            fig, ax = plt.subplots()
            ax.plot(data["relative_frequency_hz"], power_db)
            ax.set(xlabel="Frequency relative to tuned center (Hz)",
                   ylabel="Bin power (dB relative to unity; floor -200 dB)",
                   title="First 2048 IQ samples (rectangular window)")
            ax.grid(True)
            fig.savefig(args.output_dir / "spectrum.png", dpi=150, bbox_inches="tight")
            plt.close(fig)
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

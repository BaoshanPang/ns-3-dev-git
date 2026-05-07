#!/usr/bin/env python3
"""Summarize throughput values from router1_throughput.dat."""

import argparse
import sys
from pathlib import Path
from statistics import median


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compute minimum, median, and maximum throughput from router1_throughput.dat."
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default="router1_throughput.dat",
        help="Input data file. Defaults to router1_throughput.dat.",
    )
    return parser.parse_args()


def read_throughput_values(input_file):
    throughput_values = []

    with input_file.open(encoding="utf-8") as data_file:
        for line_number, line in enumerate(data_file, start=1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue

            fields = stripped.split()
            if len(fields) != 2:
                raise ValueError(
                    f"line {line_number}: expected 2 fields, found {len(fields)}"
                )

            try:
                throughput = float(fields[1])
            except ValueError as exc:
                raise ValueError(
                    f"line {line_number}: invalid throughput value {fields[1]!r}"
                ) from exc

            throughput_values.append(throughput)

    return throughput_values


def main():
    args = parse_args()
    input_file = Path(args.input_file)

    if not input_file.is_file():
        print(f"error: input file not found: {input_file}", file=sys.stderr)
        return 1

    try:
        throughput_values = read_throughput_values(input_file)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not throughput_values:
        print(f"error: no throughput samples found in {input_file}", file=sys.stderr)
        return 1

    print(f"{min(throughput_values):.3f} / {median(throughput_values):.3f} / {max(throughput_values):.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

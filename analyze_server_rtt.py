#!/usr/bin/env python3
"""Summarize RTT values from server_rtt.dat."""

import argparse
import sys
from pathlib import Path
from statistics import median


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compute minimum, median, and maximum RTT from server_rtt.dat."
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default="server_rtt.dat",
        help="Input data file. Defaults to server_rtt.dat.",
    )
    return parser.parse_args()


def read_rtt_values(input_file):
    rtt_values = []

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
                rtt = float(fields[1])
            except ValueError as exc:
                raise ValueError(
                    f"line {line_number}: invalid RTT value {fields[1]!r}"
                ) from exc

            rtt_values.append(rtt)

    return rtt_values


def main():
    args = parse_args()
    input_file = Path(args.input_file)

    if not input_file.is_file():
        print(f"error: input file not found: {input_file}", file=sys.stderr)
        return 1

    try:
        rtt_values = read_rtt_values(input_file)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not rtt_values:
        print(f"error: no RTT samples found in {input_file}", file=sys.stderr)
        return 1

    print(f"{min(rtt_values):.3f} / {median(rtt_values):.3f} / {max(rtt_values):.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

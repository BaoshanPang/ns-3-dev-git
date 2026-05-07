#!/usr/bin/env python3
"""Summarize queue size values from router1_queue.dat."""

import argparse
import sys
from pathlib import Path
from statistics import median


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compute minimum, average, and maximum queue size from router1_queue.dat."
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default="router1_queue.dat",
        help="Input data file. Defaults to router1_queue.dat.",
    )
    parser.add_argument(
        "--csv",
        action="store_true",
        help="Print comma-separated output instead of an aligned table.",
    )
    return parser.parse_args()


def read_queue_sizes(input_file):
    queue_sizes = []

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
                queue_size = int(fields[1])
            except ValueError as exc:
                raise ValueError(
                    f"line {line_number}: invalid queue size value {fields[1]!r}"
                ) from exc

            queue_sizes.append(queue_size)

    return queue_sizes


def print_csv(queue_sizes):
    print(f"{min(queue_sizes)} / {median(queue_sizes):.3f} / {max(queue_sizes)}")


def print_table(queue_sizes):
    print(f"{min(queue_sizes)} / {median(queue_sizes):.3f} / {max(queue_sizes)}")


def main():
    args = parse_args()
    input_file = Path(args.input_file)

    if not input_file.is_file():
        print(f"error: input file not found: {input_file}", file=sys.stderr)
        return 1

    try:
        queue_sizes = read_queue_sizes(input_file)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not queue_sizes:
        print(f"error: no queue samples found in {input_file}", file=sys.stderr)
        return 1

    if args.csv:
        print_csv(queue_sizes)
    else:
        print_table(queue_sizes)
    return 0


if __name__ == "__main__":
    sys.exit(main())

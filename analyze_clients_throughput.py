#!/usr/bin/env python3
"""Summarize per-client throughput from clients_throughput.dat."""

import argparse
import shlex
import sys
from collections import defaultdict
from statistics import median
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Compute minimum, average, and maximum throughput for each host in "
            "clients_throughput.dat."
        )
    )
    parser.add_argument(
        "input_file",
        nargs="?",
        default="clients_throughput.dat",
        help="Input data file. Defaults to clients_throughput.dat.",
    )
    parser.add_argument(
        "--csv",
        action="store_true",
        help="Print comma-separated output instead of an aligned table.",
    )
    return parser.parse_args()


def read_throughput_by_host(input_file):
    throughput_by_host = defaultdict(list)

    with input_file.open(encoding="utf-8") as data_file:
        for line_number, line in enumerate(data_file, start=1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue

            try:
                fields = shlex.split(stripped)
            except ValueError as exc:
                raise ValueError(f"line {line_number}: could not parse row: {exc}") from exc

            if len(fields) != 3:
                raise ValueError(
                    f"line {line_number}: expected 3 fields, found {len(fields)}"
                )

            host = fields[1]
            try:
                throughput_mbps = float(fields[2])
            except ValueError as exc:
                raise ValueError(
                    f"line {line_number}: invalid throughput value {fields[2]!r}"
                ) from exc

            throughput_by_host[host].append(throughput_mbps)

    return throughput_by_host


def ip_sort_key(host):
    parts = host.split(".")
    if len(parts) == 4:
        try:
            return tuple(int(part) for part in parts)
        except ValueError:
            pass
    return (host,)


def summarize(throughput_by_host):
    summaries = []
    for host, values in throughput_by_host.items():
        summaries.append(
            {
                "host": host,
                "count": len(values),
                "min": min(values),
                "avg": sum(values) / len(values),
                "max": max(values),
            }
        )
    return sorted(summaries, key=lambda item: ip_sort_key(item["host"]))


def print_csv(summaries):
    print("host,count,min_mbps,avg_mbps,max_mbps")
    for item in summaries:
        print(
            f"{item['host']},{item['count']},"
            f"{item['min']:.3f},{item['avg']:.3f},{item['max']:.3f}"
        )
    print_average_summary_csv(summaries)


def print_table(summaries):
    print(f"{'Host':<15} {'Samples':>7} {'Min Mbps':>10} {'Avg Mbps':>10} {'Max Mbps':>10}")
    print(f"{'-' * 15} {'-' * 7:>7} {'-' * 10:>10} {'-' * 10:>10} {'-' * 10:>10}")
    for item in summaries:
        print(
            f"{item['host']:<15} {item['count']:>7} "
            f"{item['min']:>10.3f} {item['avg']:>10.3f} {item['max']:>10.3f}"
        )
    print_average_summary_table(summaries)


def average_values(summaries):
    return [item["avg"] for item in summaries]


def print_average_summary_csv(summaries):
    averages = average_values(summaries)
    print()
    print("statistic,value_mbps")
    print(f"median_avg_mbps,{median(averages):.3f}")
    print(f"min_avg_mbps,{min(averages):.3f}")
    print(f"max_avg_mbps,{max(averages):.3f}")


def print_average_summary_table(summaries):
    averages = average_values(summaries)
    print()
    print("Summary of host average throughput values:")
    print(f"Median Avg Mbps: {median(averages):.3f}")
    print(f"Min Avg Mbps:    {min(averages):.3f}")
    print(f"Max Avg Mbps:    {max(averages):.3f}")
    print()
    print(f"{min(averages):.3f} / {median(averages):.3f} / {max(averages):.3f}")


def main():
    args = parse_args()
    input_file = Path(args.input_file)

    if not input_file.is_file():
        print(f"error: input file not found: {input_file}", file=sys.stderr)
        return 1

    try:
        throughput_by_host = read_throughput_by_host(input_file)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not throughput_by_host:
        print(f"error: no throughput samples found in {input_file}", file=sys.stderr)
        return 1

    summaries = summarize(throughput_by_host)
    if args.csv:
        print_csv(summaries)
    else:
        print_table(summaries)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env sh

printf "%-12s %-28s %-20s %-24s\n" \
    "config" \
    "clients throughput" \
    "router1 queue size" \
    "aggregate throughput"
printf "%-12s %-28s %-20s %-24s\n" \
    "------------" \
    "----------------------------" \
    "--------------------" \
    "------------------------"

for t in 0.1 0.2 0.5 0.8 1.0 2.0 10.0; do
    if [ -d "t$t" ]; then
        cthr=$(./analyze_clients_throughput.py "t$t/clients_throughput.dat" | tail -n1)
        qsz=$(./analyze_router1_queue.py "t$t/router1_queue.dat" | tail -n1)
        athr=$(./analyze_router1_throughput.py "t$t/router1_throughput.dat")
        printf "%-12s %-28s %-20s %-24s\n" "ecn_$t" "$cthr" "$qsz" "$athr"
    fi
done

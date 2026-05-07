#!/usr/bin/env sh

printf "%-10s %-24s\n" "config" "clients throughput"
printf "%-10s %-24s\n" "" "(MinAvg/MedAvg/MaxAvg)"
printf "%-10s %-24s\n" "----------" "------------------------------"

for t in 0.1 0.2 0.5 0.8 1.0 2.0 10.0; do
    if [ -d "t$t" ]; then
        res=$(./analyze_clients_throughput.py "t$t/clients_throughput.dat" | tail -n1)
        printf "ecn_%-10s %-24s\n" "$t" "$res"
    fi
done

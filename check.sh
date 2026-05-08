#!/usr/bin/env sh

print_row() {
    label=$1
    base_dir=$2

    cthr=$(./analyze_clients_throughput.py "$base_dir/clients_throughput.dat" | tail -n1)
    qsz=$(./analyze_router1_queue.py "$base_dir/router1_queue.dat" | tail -n1)
    athr=$(./analyze_router1_throughput.py "$base_dir/router1_throughput.dat")
    srtt=$(./analyze_server_rtt.py "$base_dir/server_rtt.dat")
    printf "%-12s %-28s %-20s %-28s %-24s\n" "$label" "$cthr" "$qsz" "$athr" "$srtt"
}

printf "%-12s %-28s %-20s %-28s %-24s\n" \
    "config" \
    "clients throughput" \
    "router1 queue size" \
    "aggregate throughput" \
    "server rtt"
printf "%-12s %-28s %-20s %-28s %-24s\n" \
    "------------" \
    "----------------------------" \
    "--------------------" \
    "----------------------------" \
    "------------------------"

for t in 0.1 0.2 0.5 0.8 1.0 2.0 10.0; do
    if [ -d "t$t" ]; then
        print_row "ecn_$t" "t$t"
    fi
done

if [ -d "no_ecn" ]; then
    print_row "no_ecn" "no_ecn"
fi

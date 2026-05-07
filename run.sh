#!/usr/bin/env sh

# 1. router 1 queue size
# 2. agucate throughput
# 3. rtt
for c in 30; do
    for t in 0.1 0.2 0.5 0.8 1.0 2.0 10.0;do
        set -x
        date
        ./ns3 run tcp-cubic-media-server -- -nClients=30  -markThreshold=$t --useEcn=true
        date
        mkdir -p t$t
        cp -f *.dat t$t
        set +x
    done
done

date
./ns3 run tcp-cubic-media-server -- -nClients=30  -markThreshold=$t --useEcn=false
mkdir -p no_ecn
cp -f *.dat no_ecn
date

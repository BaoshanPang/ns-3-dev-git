#!/usr/bin/env sh

for c in 1000; do
    for t in 0.1 0.2 0.5 0.8 1.0 2.0 10.0;do
        set -x
        date
        ./ns3 run tcp-cubic-media-server -- -nClients=$c  -markThreshold=$t --useEcn=true
        date
        mkdir -p t$t
        cp -f *.dat t$t
        set +x
    done
    date
    ./ns3 run tcp-cubic-media-server -- -nClients=$c  --useEcn=false
    mkdir -p no_ecn
    cp -f *.dat no_ecn
    date
done

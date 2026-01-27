#!/usr/bin/env sh

rm -fr bbr-results-2to1
for e in 0 1; do
    for id in TcpBbr TcpNewReno; do
        for q in FqCoDelQueueDisc FifoQueueDisc; do
            echo ./ns3 run "tcp-bbr-example-2to1 --tcpTypeId=$id --queueDisc=$q --queueUseEcn=$e"
            ./ns3 run "tcp-bbr-example-2to1 --tcpTypeId=$id --queueDisc=$q --queueUseEcn=$e"
        done
    done
done
set -v
gnuplot -c throughput.gp `find bbr-results-2to1 -name throughput.dat|sort|xargs echo|sort`
gnuplot -c queuesize.gp `find bbr-results-2to1 -name queueSize.dat|sort|xargs echo|sort`
sort bbr-results-2to1/txbytes > tmp; cp -f tmp bbr-results-2to1/txbytes
gnuplot txbytes_bar.gp
set +v

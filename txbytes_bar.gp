set terminal pngcairo size 1024,768 enhanced color font "Serif,12"
set output "txbytes.png"

set style data histograms
set style fill solid 0.8 border -1
set boxwidth 0.7

set ylabel "TotalTXBytes"
set xlabel "config"

set grid ytics
set xtics rotate by -30

unset key

plot "bbr-results-2to1/txbytes" using 2:xtic(1)

# -----------------------------
# Usage check
# -----------------------------
if (ARGC < 1) {
    print "Usage: gnuplot -c plot.gp <file1> [file2 ...]"
    exit
}

# -----------------------------
# Output settings
# -----------------------------
set terminal pngcairo size 1024,768 enhanced color lw 1.5
set output "throughput.png"

set grid xtics ytics ls 12 lc rgb 'gray'
set xlabel "Time (s)"
set ylabel "Throughput (Mbps)"
set xrange [0:100]
set yrange [0:20]
set title "Network Throughput Comparison"
set key outside right top

# -----------------------------
# Improved Helper function
# Returns the parent folder name or the filename if no folder exists
# -----------------------------
get_title(n) = system(sprintf("basename $(dirname %s)", value(sprintf("ARG%d", n))))

# -----------------------------
# Plotting
# -----------------------------
plot for [i=1:ARGC] \
     value(sprintf("ARG%d", i)) \
     using 1:2 \
     with lines \
     title get_title(i)

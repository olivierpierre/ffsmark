#!/bin/sh
# Real-time flash access plot (uses the spatial view /proc/flashmon)
# Usage : load flashmon, then this script, and do some things on your
# traced flash chip / partition to see the numbers of R/W/E changing  

drive_gnuplot(){
sleep 2

plot_str="plot '/proc/flashmon' using 1 ti 'reads', '' using 2 ti 'writes', '' u 3 ti 'erase'"

echo "set title 'Flash physical access occurences'"
echo "set xlabel 'Physical block number'"
echo "set yalbel 'Operation occurences'"

echo $plot_str

while true; do
sleep 1
echo replot
done
}

echo "Running gnuplot real-time plot"
drive_gnuplot | gnuplot

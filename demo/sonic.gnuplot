#! /bin/gnuplot
set terminal postscript eps color enhanced "Times-Roman" 24

lm=6.25
yo=0.85
bm=2.75
xo=0.5

set boxwidth 0.8

set xlabel 'Interpacket Delay (ns)' font ',20'
set ylabel 'Probability Distribution' font ',20'

#cdf first
set output output_file2
plot input_file using 2:5 with linespoints lc rgb "blue" notitle

#pdf
set format y "10^{%L}"
set logscale y
set yrange [0.000001 : 1]

#if (xstart != 0 && xend != 0) {
set xrange [xstart:xend]
#}

set output output_file1
plot input_file using 2:4 with boxes lc rgb "blue" notitle


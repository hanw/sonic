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
#set output output_file2
#plot input_file using 2:5 with linespoints lc rgb "blue" notitle

#pdf
set format y "10^{%L}"
set logscale y
set yrange [0.00001 : 1]
set xrange [11000:13500]

set style fill solid

set output output_file
plot input_file1 using 2:4 with boxes title "Zero", \
    input_file2 using 2:4 with boxes title "One", \
    "<echo '12416.0 0.5'" with boxes fs pattern 0.1 lc rgb "black" notitle,\
    "<echo '12006.4 0.5'" with boxes fs pattern 0.1 lc rgb "black" notitle,\
    "<echo '12211.2 1'" with boxes fs pattern 0.1 lc rgb "red" notitle


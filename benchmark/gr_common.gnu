;set term svg enhanced font 'arial,10' size 800,800
;bext = ".svg"
set terminal png nocrop enhanced font arial 10 size 800,800
bext = ".png"
set xtics nomirror rotate by -45 font "arial,8"
set key below
set style data linespoints
set datafile missing "0"
set xlabel "Number of elements in logarithmic scale"
set ylabel "Time for element in nanosecond in logarithmic scale\nLower is better"
set xrange [1000:10000000]
set yrange [6:1000]
set logscale y
set logscale x
set format y "%.0fns"
set format x "%.0s%c"
bdir = "data/"


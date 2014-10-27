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

set style line 1 lc 1 lt 1
set style line 2 lc 2 lt 2
set style line 3 lc 3 lt 3
set style line 4 lc 4 lt 4
set style line 5 lc 5 lt 5
set style line 6 lc 6 lt 6
set style line 7 lc 7 lt 7
set style line 8 lc 8 lt 8
set style line 9 lc 9 lt 9
set style line 10 lc 10 lt 10
set style line 11 lc 11 lt 11
set style line 12 lc 12 lt 12
set style line 13 lc 13 lt 13
set style line 14 lc 14 lt 14
set style line 15 lc 15 lt 15
set style line 16 lc 16 lt 16
set style line 17 lc 17 lt 17
set style line 18 lc 18 lt 18
set style line 19 lc 19 lt 19
set style line 20 lc 20 lt 20




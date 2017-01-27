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

# for some colors see: http://www.uni-hamburg.de/Wiss/FB/15/Sustainability/schneider/gnuplot/colors.htm
set style line 1 lc 1 lt 1 # hashtable
set style line 2 lc 2 lt 2 # hashdyn
set style line 3 lc 3 lt 3 # hashlin
set style line 4 lc 4 lt 4 # trie
set style line 5 lc 5 lt 5 # trie-inplace
set style line 6 lc 6 lt 6 # rbtre
set style line 7 lc 7 lt 7 # nedtrie
set style line 8 lc 8 lt 8 # khash
set style line 9 lc 9 lt 8 # uthash
set style line 10 lc 10 lt 10 # judy
set style line 11 lc 11 lt 11 # judyarray
set style line 12 lc 12 lt 12 # googledensehash
set style line 13 lc rgb "#FF69B4" lt 13 # googlebtree
set style line 14 lc 14 lt 14 # stxbtree
set style line 15 lc 15 lt 15 # c++unorderedmap
set style line 16 lc 16 lt 16 # c++map
set style line 17 lc 17 lt 17 # tesseract
set style line 18 lc 4 lt 18 pt 1 # libdynamic
set style line 19 lc rgb "#FF69B4" lt 19 # googlelibchash
set style line 20 lc rgb "#1E90FF" lt 20 # concurrencykit


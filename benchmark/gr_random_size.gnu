load "gr_common.gnu"

set output bdir.tdir."img_random_size".bext
set title "Size".tsub
set format y "%.0f"
set ylabel "Size for element in byte\nLower is better"
unset logscale y
set yrange [0:80]
data = bdir.tdir.'dat_random_size.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


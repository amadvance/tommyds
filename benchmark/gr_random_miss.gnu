load "gr_common.gnu"

set output bdir.tdir."img_random_miss".bext
set title "Random Miss".tsub
data = bdir.tdir.'dat_random_miss.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


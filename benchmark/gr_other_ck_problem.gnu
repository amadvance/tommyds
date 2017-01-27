load "gr_common.gnu"

set yrange [10:10000]

tdir = "other/"
tsub = "\nCore i5 650 3.20 GHz, 4 MB L3 cache\nLinux, gcc 4.7.1, 32 bit"

set output bdir.tdir."ck_problem".bext
set title "Random Change (Remove + Insert)".tsub
data = bdir.tdir.'ck_problem.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


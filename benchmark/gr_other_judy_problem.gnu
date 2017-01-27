load "gr_common.gnu"

set yrange [10:10000]

tdir = "other/"
tsub = "\nXeon E5430 2.66 GHz, 2x6 MB L2 cache, 1333 MT/s FSB\nWindows, Visual C 2008, 32 bit"

set output bdir.tdir."judy_problem".bext
set title "Forward Change (Remove + Insert)".tsub
data = bdir.tdir.'judy_problem.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


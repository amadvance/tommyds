load "gr_common.gnu"

tdir = "other/"
tsub = "\nCore 2 Duo E6600 2.40 GHz, 4 MB L2 cache, 1066 MT/s FSB\nWindows, Visual C 2008, 32 bit"

set output bdir.tdir."cgoogledensehash_problem".bext
set title "Random Change (Remove + Insert)".tsub
data = bdir.tdir.'cgoogledensehash_problem.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:16] '' using 1:i title columnheader(i)

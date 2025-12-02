load "gr_common.gnu"

set yrange [10:10000]

tdir = "other/"
tsub = "\nCore i7 10700 2.90 GHz, 16 MB L3 cache\nLinux, gcc 15.2, 64 bit" 

set output bdir.tdir."googledensehash_problem".bext
set title "Random Change (Remove + Insert)".tsub
data = bdir.tdir.'googledensehash_problem.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


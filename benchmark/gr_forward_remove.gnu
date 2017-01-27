load "gr_common.gnu"

set output bdir.tdir."img_forward_remove".bext
set title "Forward Remove".tsub
data = bdir.tdir.'dat_forward_remove.lst'

plot data using 1:2 title columnheader(2), \
	for [i=3:21] '' using 1:i title columnheader(i) ls i-1


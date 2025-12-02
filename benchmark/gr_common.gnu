# Set data dir and output extension
bdir = "data/"
bext = ".png"

# Set the output terminal to PNG, disable cropping, enable enhanced text mode (for subscripts/superscripts), 
# specify font family and base size (12pt), and set the output image size to 1280x900 pixels.
set terminal png nocrop enhanced font arial 12 size 1280,900

# Configure the Y-axis tics (tick marks and labels): hide the mirror tics on the opposite side 
# and set the font for the tic labels to Arial, 8pt.
set ytics nomirror font "arial,8"

# Configure the X-axis tics: hide the mirror tics, rotate the tic labels by -45 degrees 
# to prevent overlap, and set the font for the tic labels to Arial, 8pt.
set xtics nomirror rotate by -45 font "arial,8"

# Position the key (legend) below the plot area.
set key below

# Set the default drawing style for data points to lines connecting the points.
set style data linespoints

# Instruct gnuplot to treat the string "0" in the data file as missing data, 
# preventing lines from being drawn across these points.
set datafile missing "0"

# Set the label for the X-axis.
set xlabel "Number of elements in logarithmic scale"

# Set the label for the Y-axis.
set ylabel "Time for element in nanosecond in logarithmic scale\nLower is better"

# Set the visible range for the X-axis, from 1000 to 10,000,000.
set xrange [1000:10000000]

# Set the visible range for the Y-axis, from 6 to 1000.
set yrange [6:1000]

# Apply a logarithmic scale to the Y-axis (base 10 by default).
set logscale y

# Apply a logarithmic scale to the X-axis (base 10 by default).
set logscale x

# Define the format for the Y-axis tic labels: display as a whole number (%.0f) 
# followed by the unit "ns" (nanoseconds).
set format y "%.0fns"

# Define the format for the X-axis tic labels using engineering notation/SI prefixes: 
# display as a whole number (%.0s) followed by the appropriate SI prefix (%c). 
# E.g., 1000 is shown as "1k", 1000000 as "1M".
set format x "%.0s%c"

# Increase the bottom margin to create space for the long xlabel and the key below it.
# The default margin is often around 5. Try increasing it to 8 or more.
set bmargin 10

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


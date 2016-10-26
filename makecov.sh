#!/bin/sh
#
# Run the Coverage test
#

make distclean

if ! make CC=gcc COVERAGE=1 lcov_reset check lcov_capture lcov_html; then
	exit 1
fi



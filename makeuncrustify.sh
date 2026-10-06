#!/bin/sh

uncrustify -c uncrustify.cfg --replace --no-backup check.c tommyds/*.c tommyds/*.h

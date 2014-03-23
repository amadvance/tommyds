#############################################################################
# Tommy Makefile

VERSION=1.9
CFLAGS=-m32 -O3 -march=pentium4 -mtune=generic -Wall -Wextra -Wshadow -Wcast-qual -g
# -std=c++11 required by Google btree
CXXFLAGS=$(CFLAGS) -fpermissive -std=c++11
CC=gcc
CXX=g++
UNAME=$(shell uname)

# Linux
ifeq ($(UNAME),Linux)
LIB=-lrt benchmark/lib/judy/libJudyL.a benchmark/lib/judy/libJudyMalloc.a
EXE=
O=.o
endif

# Darwin
ifeq ($(UNAME),Darwin)
LIB=
EXE=
O=.o
endif

# Windows
ifeq ($(UNAME),)
LIB=benchmark/lib/judy/src/judy.lib
EXE=.exe
O=.obj
endif

CHECK = ./tommybench -N 1000000 -d tommy-hashlin
#CHECK = ./tommycheck

DEP = \
	tommyds/tommyalloc.c \
	tommyds/tommyalloc.h \
	tommyds/tommyarray.c \
	tommyds/tommyarray.h \
	tommyds/tommyarrayof.c \
	tommyds/tommyarrayof.h \
	tommyds/tommyarrayblk.c \
	tommyds/tommyarrayblk.h \
	tommyds/tommyarrayblkof.c \
	tommyds/tommyarrayblkof.h \
	tommyds/tommy.c \
	tommyds/tommy.h \
	tommyds/tommyhash.c \
	tommyds/tommyhashdyn.c \
	tommyds/tommyhashdyn.h \
	tommyds/tommyhash.h \
	tommyds/tommyhashlin.c \
	tommyds/tommyhashlin.h \
	tommyds/tommyhashtbl.c \
	tommyds/tommyhashtbl.h \
	tommyds/tommylist.c \
	tommyds/tommylist.h \
	tommyds/tommytrie.c \
	tommyds/tommytrie.h \
	tommyds/tommytrieinp.c \
	tommyds/tommytrieinp.h \
	tommyds/tommytypes.h \
	tommyds/tommychain.h

DEPTEST = \
	check.c \
	benchmark.cc

all: tommycheck$(EXE) tommybench$(EXE)

tommy$(O): $(DEP)
	$(CC) $(CFLAGS) -c tommyds/tommy.c -o tommy$(O)
	$(CC) $(CFLAGS) -S -fverbose-asm tommyds/tommy.c -o tommy.s
	objdump -S tommy$(O) > tommy.S

tommycheck$(EXE): check.c tommy$(O)
	$(CC) $(CFLAGS) check.c tommy$(O) -o tommycheck$(EXE) $(LIB)

tommybench$(EXE): benchmark.cc $(DEP)
	$(CXX) $(CXXFLAGS) benchmark.cc -o tommybench$(EXE) $(LIB)

check: tommycheck$(EXE) tommybench$(EXE)
	./tommycheck$(EXE)
	./tommybench$(EXE) -N 100000
	echo Check completed with success!

valgrind:
	valgrind \
		--tool=memcheck \
		--track-origins=yes \
		--read-var-info=yes \
		-v $(CHECK) \
		2> valgrind.log
		tail valgrind.log

callgrind:
	valgrind \
		--tool=callgrind \
		--dump-instr=yes \
		--trace-jump=yes \
		-v $(CHECK) \
		2> callgrind.log
		tail callgrind.log

cachegrind:
	valgrind \
		--tool=cachegrind \
		-v $(CHECK) \
		2> cachegrind.log
		tail cachegrind.log

phony:

graph: phony
	cd benchmark && sh gr_all.sh

doc: phony tommy.doxygen tommy.css $(DEP)
	rm -rf doc
	mkdir doc
	cp -a benchmark/data/* doc/
	rm -f doc/*/*.lst
	rm -f doc/*/*.gnu
	doxygen tommy.doxygen
	rm -f doc/doxygen.png
	rm -f doc/tab_*.png

web: phony tommyweb.doxygen tommy.css $(DEP)
	rm -rf web
	mkdir web
	cp -a benchmark/data/* web/
	rm -f web/*/*.lst
	rm -f web/*/*.gnu
	doxygen tommyweb.doxygen
	rm -f web/doxygen.png
	rm -f web/tab_*.png

clean:
	rm -f *.log *.s *.S *.lst *.o
	rm -f *.ncb *.suo *.obj
	rm -rf Debug Release x64
	rm -f callgrind.out.*
	rm -f cachegrind.out.*

distclean: clean
	rm -f tommybench$(EXE) tommycheck$(EXE)

maintainerclean: distclean
	rm -rf doc web

DIST=tommyds-$(VERSION)

DISTFILES=\
	Makefile \
	README LICENSE AUTHORS INSTALL HISTORY \
	tommy.doxygen tommy.css tommy-header.html tommy-footer.html \
	benchmark.vcxproj benchmark.sln \
	benchmark.geany \
	benchmark.cc \
	check.c

dist:
	mkdir $(DIST)
	mkdir $(DIST)/tommyds
	cp $(DISTFILES) $(DIST)
	cp $(DEP) $(DIST)/tommyds
	cp $(DEPTEST) $(DIST)
	cp -R doc $(DIST)
	cp -R benchmark $(DIST)/benchmark
	rm -f $(DIST)/benchmark/data/*/*.png
	rm -rf $(DIST)/benchmark/data/test
	rm -f $(DIST)/benchmark/arial.ttf
	rm -f $(DIST).tar.gz
	tar cfzo $(DIST).tar.gz $(DIST)
	rm -f $(DIST).zip
	zip -r $(DIST).zip $(DIST)
	rm -r $(DIST)


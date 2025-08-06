CC      = g++
CFLAGS += -Wall -Wextra -std=c++20 -pthread -fopenmp -ljemalloc
OLEVEL ?= -O2
DEBUG   = -O0 -g -fno-omit-frame-pointer -fopenmp -ljemalloc

SRCDIR = src
BINDIR = bin
TESTDIR = $(SRCDIR)/test
EXPDIR = $(SRCDIR)/experiments

# Programs array
DEBUG_SUFFIX = __debug
PROGRAMS = coverage \
           high_water_mark \
		   peak_by_bit \
		   performance_stats \
		   single_collatz \
		   tests
DEBUG_PROGRAMS = $(addsuffix $(DEBUG_SUFFIX), $(PROGRAMS))

# Targets
release:
	$(MAKE) $(PROGRAMS)

debug:
	$(MAKE) $(DEBUG_PROGRAMS)

all:
	$(MAKE) release
	$(MAKE) debug

clean:
	rm -f $(BINDIR)/*
	echo "Cleanup complete!"

coverage:
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx
coverage$(DEBUG_SUFFIX):
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/coverage$(DEBUG_SUFFIX) \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx

high_water_mark:
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx
high_water_mark$(DEBUG_SUFFIX):
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/high_water_mark$(DEBUG_SUFFIX) \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx

peak_by_bit:
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/peak_by_bit \
		$(SRCDIR)/peak_by_bit.cpp \
		-lgmp -lgmpxx
peak_by_bit$(DEBUG_SUFFIX):
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/peak_by_bit$(DEBUG_SUFFIX) \
		$(SRCDIR)/peak_by_bit.cpp \
		-lgmp -lgmpxx

performance_stats:
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/performance_stats \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx
performance_stats$(DEBUG_SUFFIX):
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/performance_stats$(DEBUG_SUFFIX) \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx

single_collatz:
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx
single_collatz$(DEBUG_SUFFIX):
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/single_collatz$(DEBUG_SUFFIX) \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx

tests:
	echo "Compiling classes and running with $(OLEVEL)."
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/test__collatz_class \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__collatz_class
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/test__node_class \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__node_class
	$(CC) $(CFLAGS) $(OLEVEL) -o $(BINDIR)/test__binary_tree_class \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__binary_tree_class
tests$(DEBUG_SUFFIX):
	echo "Compiling classes and running with $(DEBUG) (debug)."
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__collatz_class$(DEBUG_SUFFIX) \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__collatz_class$(DEBUG_SUFFIX)
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__node_class$(DEBUG_SUFFIX) \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__node_class$(DEBUG_SUFFIX)
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__binary_tree_class$(DEBUG_SUFFIX) \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__binary_tree_class$(DEBUG_SUFFIX)

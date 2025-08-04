CC     = g++
CFLAGS += -Wall -Wextra -std=c++20 -pthread -fopenmp -ljemalloc
O2     = -O2
DEBUG  = -O0 -g -fno-omit-frame-pointer -fopenmp -ljemalloc

SRCDIR = src
BINDIR = bin
TESTDIR = $(SRCDIR)/test
EXPDIR = $(SRCDIR)/experiments

# Targets
build:
	$(MAKE) clean
	$(MAKE) coverage
	$(MAKE) high_water_mark
	$(MAKE) peak_by_bit
	$(MAKE) performance_stats
	$(MAKE) single_collatz
	$(MAKE) tests

clean:
	rm -f $(BINDIR)/*
	echo "Cleanup complete!"

coverage:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/coverage__debug \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx

high_water_mark:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/high_water_mark__debug \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx

peak_by_bit:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/peak_by_bit \
		$(SRCDIR)/peak_by_bit.cpp \
		-lgmp -lgmpxx
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/peak_by_bit__debug \
		$(SRCDIR)/peak_by_bit.cpp \
		-lgmp -lgmpxx

performance_stats:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/performance_stats \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/performance_stats__debug \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx

single_collatz:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/single_collatz__debug \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx

tests:
	echo "Compiling classes and running with $(DEBUG) (debug)."
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__collatz_class__debug \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__collatz_class__debug
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__node_class__debug \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__node_class__debug
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/test__binary_tree_class__debug \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__binary_tree_class__debug
	echo "Compiling classes and running with $(O2)."
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__collatz_class \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__collatz_class
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__node_class \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__node_class
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__binary_tree_class \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__binary_tree_class

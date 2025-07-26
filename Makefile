CC     = g++
CFLAGS += -Wall -Wextra -std=c++20 -pthread
O2     = -O2
DEBUG  = -O0 -g -fno-omit-frame-pointer

SRCDIR = src
BINDIR = bin
TESTDIR = $(SRCDIR)/test
EXPDIR = $(SRCDIR)/experiments

# Targets
build:
	$(MAKE) coverage
	$(MAKE) coverage__debug
	$(MAKE) high_water_mark
	$(MAKE) performance_stats
	$(MAKE) single_collatz

clean:
	rm -f $(BINDIR)/*
	echo "Cleanup complete!"

coverage:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx

coverage__debug:
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/coverage__debug \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx

high_water_mark:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx

high_water_mark__debug:
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/high_water_mark__debug \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx

performance_stats:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/performance_stats \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx

single_collatz:
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx

single_collatz__debug:
	$(CC) $(CFLAGS) $(DEBUG) -o $(BINDIR)/single_collatz__debug \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx

tests:
	echo "Compiling classes."
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__collatz_class \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	echo "Running tests."
	$(BINDIR)/test__collatz_class
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__node_class \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	echo "Running tests."
	$(BINDIR)/test__node_class
	$(CC) $(CFLAGS) $(O2) -o $(BINDIR)/test__binary_tree_class \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	echo "Running tests."
	$(BINDIR)/test__binary_tree_class

# Build flags and such.
CC                   = g++
CFLAGS              += -Wall -Wextra -std=c++20 -pthread -fopenmp -ljemalloc -Isrc/include
OLEVEL              ?= -O2
DEBUG_EXTRA_FLAGS    = -O0 -g -fno-omit-frame-pointer
RELEASE_FLAGS        = $(CFLAGS) $(OLEVEL)
DEBUG_FLAGS          = $(CFLAGS) $(DEBUG_EXTRA_FLAGS)
DEBUG_SUFFIX         = _debug

# Directories.
SRCDIR       = src
TESTDIR      = $(SRCDIR)/test
EXPDIR       = $(SRCDIR)/experiments
BINDIR       = bin
BINDIR_DEBUG = $(BINDIR)$(DEBUG_SUFFIX)
OBJDIR       = obj
OBJDIR_DEBUG = $(OBJDIR)$(DEBUG_SUFFIX)

# Programs array.
PROGRAMS = coverage \
           high_water_mark \
		   peak_by_bit \
		   performance_stats \
		   single_collatz \
		   tests
DEBUG_PROGRAMS = $(addsuffix $(DEBUG_SUFFIX), $(PROGRAMS))

# Targets.
release:
	$(MAKE) $(PROGRAMS)

debug:
	$(MAKE) $(DEBUG_PROGRAMS)

all:
	$(MAKE) release
	$(MAKE) debug

clean:
	rm -f $(BINDIR)/*
	rm -f $(BINDIR_DEBUG)/*
	rm -f $(OBJDIR)/*
	rm -f $(OBJDIR_DEBUG)/*
	echo "Cleanup complete!"

compile_commands:
	bear -- make all

coverage:
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		$(SRCDIR)/logging.cpp \
		-lgmp -lgmpxx
coverage$(DEBUG_SUFFIX):
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/coverage \
		$(SRCDIR)/coverage.cpp \
		$(SRCDIR)/logging.cpp \
		-lgmp -lgmpxx

high_water_mark:
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		$(SRCDIR)/logging.cpp \
		-lgmp -lgmpxx
high_water_mark$(DEBUG_SUFFIX):
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		$(SRCDIR)/logging.cpp \
		-lgmp -lgmpxx

objects:
	$(CC) $(RELEASE_FLAGS) -c src/logging.cpp -o $(OBJDIR)/logging.o
	$(CC) $(DEBUG_FLAGS) -c src/logging.cpp -o $(OBJDIR_DEBUG)/logging.o

peak_by_bit:
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/peak_by_bit \
		$(SRCDIR)/peak_by_bit.cpp \
		$(OBJDIR)/logging.o \
		-lgmp -lgmpxx
peak_by_bit$(DEBUG_SUFFIX):
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/peak_by_bit \
		$(SRCDIR)/peak_by_bit.cpp \
		$(SRCDIR)/logging.cpp \
		-lgmp -lgmpxx

performance_stats:
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/performance_stats \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx
performance_stats$(DEBUG_SUFFIX):
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/performance_stats \
		$(SRCDIR)/performance_stats.cpp \
		-lgmp -lgmpxx

single_collatz:
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx
single_collatz$(DEBUG_SUFFIX):
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx

tests:
	echo "Compiling classes and running with $(OLEVEL) for release..."
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/test__collatz_class \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__collatz_class
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/test__node_class \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__node_class
	$(CC) $(RELEASE_FLAGS) -o $(BINDIR)/test__binary_tree_class \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR)/test__binary_tree_class
tests$(DEBUG_SUFFIX):
	echo "Compiling classes and running with $(DEBUG_EXTRA_FLAGS) for debug..."
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/test__collatz_class \
		$(TESTDIR)/collatz_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR_DEBUG)/test__collatz_class
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/test__node_class \
		$(TESTDIR)/node_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR_DEBUG)/test__node_class
	$(CC) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/test__binary_tree_class \
		$(TESTDIR)/binary_tree_class.cpp \
		-lgmp -lgmpxx
	$(BINDIR_DEBUG)/test__binary_tree_class

#
# Build flags and such.
#
CXX                  = g++
CXXFLAGS            += -Wall -Wextra -std=c++20 -pthread -fopenmp -ljemalloc -Isrc/include
OLEVEL              ?= -O2
DEBUG_EXTRA_FLAGS    = -O0 -g -fno-omit-frame-pointer -fno-inline -fno-elide-constructors
RELEASE_FLAGS        = $(CXXFLAGS) $(OLEVEL)
DEBUG_FLAGS          = $(CXXFLAGS) $(DEBUG_EXTRA_FLAGS)
DEBUG_SUFFIX         = _debug

#
# Directories.
#
SRCDIR       = src
TESTDIR      = $(SRCDIR)/test
EXPDIR       = $(SRCDIR)/experiments
BINDIR       = bin
BINDIR_DEBUG = $(BINDIR)$(DEBUG_SUFFIX)
OBJDIR       = obj
OBJDIR_DEBUG = $(OBJDIR)$(DEBUG_SUFFIX)

#
# Program and test names.
#
TESTS = collatz_class \
	node_class \
	binary_tree_class
DEBUG_TESTS = $(addsuffix $(DEBUG_SUFFIX), $(TESTS))
PROGRAMS = ancestor_coverage \
	coverage \
    high_water_mark \
	peak_by_bit \
	performance_stats \
	single_collatz
DEBUG_PROGRAMS = $(addsuffix $(DEBUG_SUFFIX), $(PROGRAMS))
OBJECTS = $(OBJDIR)/logging.o
DEBUG_OBJECTS = $(OBJDIR_DEBUG)/logging.o
PROGRAM_OBJECTS = $(addprefix $(OBJDIR)/,$(addsuffix .o, $(PROGRAMS)))
DEBUG_PROGRAM_OBJECTS = $(addprefix $(OBJDIR_DEBUG)/, $(addsuffix $(DEBUG_SUFFIX).o, $(PROGRAMS)))


#
# Composite and Specialty Targets
#
.PHONY: all clean compile_commands.json release
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
	@echo "Cleanup complete!"


#
# Bear and Compile-Only Operations
#
.PHONY: compile
compile: $(OBJECTS) $(DEBUG_OBJECTS)
	@for prog in $(PROGRAMS); do \
		$(CXX) $(RELEASE_FLAGS) -c $(SRCDIR)/$$prog.cpp -o $(OBJDIR)/$$prog.o; \
	done
	@for prog in $(PROGRAMS); do \
		$(CXX) $(DEBUG_FLAGS) -c $(SRCDIR)/$$prog.cpp -o $(OBJDIR_DEBUG)/$$prog.o; \
	done

# Compile Commands expects *only* compile flags, not linking.
compile_commands.json:
	$(MAKE) clean
	bear -- make compile


#
# Object Targets
#
.PHONY: objects
objects: $(OBJDIR) $(OBJDIR_DEBUG)
	$(MAKE) $(OBJECTS) $(DEBUG_OBJECTS)

$(PROGRAM_OBJECTS) $(OBJECTS): $(SRCDIR)/logging.cpp | $(OBJDIR)
	$(CXX) $(RELEASE_FLAGS) -c $< -o $@

$(DEBUG_OBJECTS): $(SRCDIR)/logging.cpp | $(OBJDIR_DEBUG)
	$(CXX) $(DEBUG_FLAGS) -c $< -o $@


#
# Program Targets
#
$(PROGRAMS): $(OBJECTS) | $(BINDIR)
	$(CXX) $(RELEASE_FLAGS) -o $(BINDIR)/$@ \
		$(SRCDIR)/$@.cpp \
		$^ \
		-lgmp -lgmpxx

$(DEBUG_PROGRAMS): $(DEBUG_OBJECTS) | $(BINDIR_DEBUG)
	$(CXX) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/$@ \
		$(SRCDIR)/$(subst $(DEBUG_SUFFIX),,$(@)).cpp \
		$^ \
		-lgmp -lgmpxx



#
# Test Targets
#
.PHONY: tests
tests:
	$(MAKE) $(TESTS)
	$(MAKE) $(DEBUG_TESTS)

$(TESTS): $(OBJECTS) | $(BINDIR)
	@echo "Compiling and running tests for release..."
	$(CXX) $(RELEASE_FLAGS) -o $(BINDIR)/test__$@ \
		$(SRCDIR)/test/$@.cpp \
		$^ \
		-lgmp -lgmpxx
	$(BINDIR)/test__$@

$(DEBUG_TESTS): $(DEBUG_OBJECTS) | $(BINDIR_DEBUG)
	@echo "Compiling and running tests for debug..."
	$(CXX) $(DEBUG_FLAGS) -o $(BINDIR_DEBUG)/test__$(subst $(DEBUG_SUFFIX),,$(@)) \
		$(SRCDIR)/test/$(subst $(DEBUG_SUFFIX),,$(@)).cpp \
		$^ \
		-lgmp -lgmpxx
	$(BINDIR_DEBUG)/test__$(subst $(DEBUG_SUFFIX),,$(@))


#
# Directory Creation
#
$(OBJDIR) $(OBJDIR_DEBUG) $(BINDIR) $(BINDIR_DEBUG):
	mkdir -p $@

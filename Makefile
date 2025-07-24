CC     = g++
CFLAGS += -Wall -Wextra -std=c++20 -pthread -O2

SRCDIR = src
BINDIR = bin
EXPDIR = $(SRCDIR)/experiments

###SOURCES := $(wildcard $(SRCDIR)/*.c)

# Targets
build:
	$(MAKE) coverage
	$(MAKE) high_water_mark
	$(MAKE) single_collatz

clean:
	rm -f $(BINDIR)/*
	echo "Cleanup complete!"

coverage:
	$(CC) $(CFLAGS) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		-lgmp -lgmpxx

high_water_mark:
	$(CC) $(CFLAGS) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp -lgmpxx

peak_by_bit:
	$(CC) $(CFLAGS) -o $(BINDIR)/peak_by_bit \
		$(SRCDIR)/peak_by_bit.cpp \
		-lgmp -lgmpxx

single_collatz:
	$(CC) $(CFLAGS) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp \
		-lgmp -lgmpxx


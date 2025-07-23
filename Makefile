CC     = g++
CFLAGS += -Wall -Wextra -std=c++20 -pthread -O2

SRCDIR = src
BINDIR = bin
EXPDIR = $(SRCDIR)/experiments

###SOURCES := $(wildcard $(SRCDIR)/*.c)

# Targets
build:
	$(MAKE) 3n1

single_collatz:
	$(CC) $(CFLAGS) -o $(BINDIR)/single_collatz \
		$(SRCDIR)/single_collatz.cpp

coverage:
	$(CC) $(CFLAGS) -o $(BINDIR)/coverage \
		$(SRCDIR)/coverage.cpp \
		-lgmp

high_water_mark:
	$(CC) $(CFLAGS) -o $(BINDIR)/high_water_mark \
		$(SRCDIR)/high_water_mark.cpp \
		-lgmp

clean:
	rm -f $(BINDIR)/*
	echo "Cleanup complete!"

# makefile for snapper
#
# Frank Fleschner, (c) ACS, Inc, 2008

# C compiler
CC = gcc

# Program names
SNAPPER_PROGNAME = snapper
CLOP_PROGNAME = clop

# Compiler flags:
CFLAGS = -Wall

# Linker flags.
LFLAGS = 

# Required object files for each program
SNAPPER_OBJFILES = snapper.o configfile.o comm.o snap_record.o
CLOP_OBJFILES = clop.o comm.o

default: all

all: snapper clop

clean:
	rm -f *.o $(SNAPPER_PROGNAME) $(CLOP_PROGNAME) $(SNAPDIFF_PROGNAME) depend

snapper: $(SNAPPER_OBJFILES)
	$(CC) $(CFLAGS) -o $(SNAPPER_PROGNAME) $(SNAPPER_OBJFILES) $(LFLAGS)

clop: $(CLOP_OBJFILES)
	$(CC) $(CFLAGS) -o $(CLOP_PROGNAME) $(CLOP_OBJFILES) $(LFLAGS)

depend:
	$(CC) -MM *.c > depend

-include depend

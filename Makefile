# --------------------------------------------------------------------
# Makefile for COS 217 Assignment 3
# Builds:
#   testsymtablelist  (linked-list implementation)
#   testsymtablehash  (hash-table implementation)
#
# Uses gcc217 with C90 flags.
# --------------------------------------------------------------------

CC = gcc217
CFLAGS = -Wall -Wextra -std=c90 -pedantic

# --------------------------------------------------------------------
# Step 8 requirement:
# The first rule must build both executables.
# --------------------------------------------------------------------

all: testsymtablelist testsymtablehash

# --------------------------------------------------------------------
# Link the testsymtablelist executable from its object files.
# --------------------------------------------------------------------

testsymtablelist: testsymtable.o symtablelist.o
	$(CC) $(CFLAGS) testsymtable.o symtablelist.o -o testsymtablelist

# --------------------------------------------------------------------
# Link the testsymtablehash executable from its object files.
# --------------------------------------------------------------------

testsymtablehash: testsymtable.o symtablehash.o
	$(CC) $(CFLAGS) testsymtable.o symtablehash.o -o testsymtablehash

# --------------------------------------------------------------------
# Compile object files.
# Each .o depends on the .c file AND any headers it includes.
# --------------------------------------------------------------------

testsymtable.o: testsymtable.c symtable.h
	$(CC) $(CFLAGS) -c testsymtable.c

symtablelist.o: symtablelist.c symtable.h
	$(CC) $(CFLAGS) -c symtablelist.c

symtablehash.o: symtablehash.c symtable.h
	$(CC) $(CFLAGS) -c symtablehash.c

# --------------------------------------------------------------------
# Utility target to clean up build artifacts.
# This is not required by the spec but is super standard.
# --------------------------------------------------------------------

clean:
	rm -f *.o testsymtablelist testsymtablehash

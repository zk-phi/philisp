# options
CC = gcc
OPT = -g -O2 -ansi -pedantic -Wall -W -Wextra -Wunreachable-code

# directories
HEADDIR = include/
SRCDIR = src/
LIBSRCDIR = src/lib/
LIBDIR = lib/
BINDIR = bin/

# sources
HEAD = $(wildcard $(HEADDIR)*.h)
SRC = $(wildcard $(SRCDIR)*.c)
LIBSRC = $(wildcard $(LIBSRCDIR)*.c)

# targets
LIB = $(LIBSRC:$(LIBSRCDIR)%.c=$(LIBDIR)%.so)
IMPLIB = $(LIBDIR)philisp.lib # required to compile libraries on Windows
EXEC = $(BINDIR)philisp

# ----

all : $(EXEC) $(LIB)

ifeq ($(OS),Windows_NT)

$(EXEC) : $(SRC) $(HEAD)
	-mkdir bin/
	-mkdir lib/
	$(CC) $(OPT) -o $@ -rdynamic -I $(HEADDIR) $(SRC) -Wl,--out-implib,$(IMPLIB) -ldl

$(IMPIB) : $(EXEC)

$(LIBDIR)%.so : $(LIBSRCDIR)%.c $(HEAD) $(IMPLIB)
	-mkdir lib/
	$(CC) $(OPT) -o $@ -shared -fPIC -L $(LIBDIR) -I $(HEADDIR) $< -lm -lphilisp

else

$(EXEC) : $(SRC) $(HEAD)
	-mkdir bin/
	$(CC) $(OPT) -o $@ -rdynamic -I $(HEADDIR) $(SRC) -ldl

$(LIBDIR)%.so : $(LIBSRCDIR)%.c $(HEAD)
	-mkdir lib/
	$(CC) $(OPT) -o $@ -shared -fPIC -I $(HEADDIR) $< -lm

endif

# ----

.PHONY : clean

clean :
	-rm $(EXEC) $(LIB) $(IMPLIB)
	-rmdir $(LIBDIR) $(BINDIR)

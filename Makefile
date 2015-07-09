# options
CC = gcc
OPT = -O2 -ansi -pedantic -Wall -W -Wextra -Wunreachable-code -g

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

.PHONY : all clean

all : $(EXEC) $(LIB)

clean :
	-rm $(EXEC) $(LIB) $(IMPLIB)

# ----

ifeq ($(OS),Windows_NT)
$(EXEC) : $(SRC) $(HEAD) $(BINDIR) $(LIBDIR)
	$(CC) $(OPT) -o $@ -rdynamic -I $(HEADDIR) $(SRC) -Wl,--out-implib,$(IMPLIB) -ldl
else
$(EXEC) : $(SRC) $(HEAD) $(BINDIR)
	$(CC) $(OPT) -o $@ -rdynamic -I $(HEADDIR) $(SRC) -ldl
endif

ifeq ($(OS),Windows_NT)
$(LIBDIR)%.so : $(LIBSRCDIR)%.c $(LIBDIR) $(HEAD) $(IMPLIB)
	$(CC) $(OPT) -o $@ -shared -fPIC -L $(LIBDIR) -I $(HEADDIR) $< -lm -lphilisp
else
$(LIBDIR)%.so : $(LIBSRCDIR)%.c $(LIBDIR) $(HEAD)
	$(CC) $(OPT) -o $@ -shared -fPIC -I $(HEADDIR) $< -lm
endif

%/ :
	mkdir $@

# options
CC = gcc
OPT = -O2 -ansi -pedantic -Wall -W -Wextra -Wunreachable-code

# sources
HEAD = $(wildcard include/*.h)
SRC = $(wildcard src/*.c)
LIBRC = $(wildcard src/lib/*.c)

# targets
LIB = $(LIBRC:src/lib/%.c=lib/%.so)
EXEC = bin/philisp

# ----

all : $(EXEC) $(LIB)

$(EXEC) : $(SRC) $(HEAD)
	$(CC) $(OPT) -o $(EXEC) -I ./include/ $(SRC) -ldl

lib/%.so : src/lib/%.c $(HEAD)
	$(CC) $(OPT) -o $@ -shared -fPIC -I ./include/ $< ./src/philisp.c -lm

.PHONY : clean

clean :
	rm $(EXEC) $(LIB)

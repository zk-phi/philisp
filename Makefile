CC = gcc
OPT = -O2 -ansi -pedantic -Wall -W -Wextra -Wunreachable-code

all :
	-mkdir lib
	-mkdir bin
	$(CC) $(OPT) -o ./bin/philisp -I ./include/ ./src/main.c ./src/core.c ./src/philisp.c ./src/subr.c -ldl
	$(CC) $(OPT) -o ./lib/libmath.a -shared -fPIC -I ./include/ ./src/lib/libmath.c ./src/philisp.c -lm

.PHONY : clean

clean :
	-rm -r ./bin/philisp
	-rm -r ./lib/libmath.a

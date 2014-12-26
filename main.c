/* phi-lisp interpreter (prototype) : 2014 zk_phi */

#include "philisp.h"
#include "subr.h"

#include <stdio.h>

void print(FILE*, lobj);
lobj read();
lobj eval(lobj);
lobj current_env;

int main(void)
{
    subr_initialize();

    while(1)
    {
        printf(">> "); fflush(stdout);
        print(stdout, eval(read()));
        puts("\n"); fflush(stdout);
    }
    return 0;
}

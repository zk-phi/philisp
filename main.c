/* phi-lisp interpreter (prototype) : 2014 zk_phi */

#include "philisp.h"
#include "subr.h"

#include <stdio.h>

lobj eval(lobj);
void print(FILE*, lobj);
lobj read();

int main(void)
{
    /* lobj repl = */
    /*     list(2, list(3, intern("fn"), list(1, intern("loop")), */
    /*                  list(2, intern("loop"), */
    /*                       list(3, intern("fn"), NIL, */
    /*                            list(2, list(3, intern("fn"), list(1, intern("_")), */
    /*                                         list(2, intern("puts"), string("\n\n"))), */
    /*                                 list(2, list(3, intern("fn"), list(1, intern("_")), */
    /*                                              list(2, intern("print"), */
    /*                                                   list(2, intern("eval"), */
    /*                                                        list(1, intern("read"))))), */
    /*                                      list(2, intern("puts"), string(">> "))))))), */
    /*          list(3, intern("fn"), list(1, intern("f")), */
    /*               list(2, list(3, intern("fn"), list(1, intern("_")), */
    /*                            list(2, intern("loop"), intern("f"))), */
    /*                    list(1, intern("f"))))); */

    /* ((fn (loop) */
    /*   (loop (fn () ((fn (_) (puts "\n\n")) */
    /*                 ((fn (_) (print (eval (read)))) */
    /*                  (puts ">> ")))))) */
    /*  (fn (f) ((fn (_) (loop f)) (f)))) */

    subr_initialize();

    /* (subr_eval.function)(list(1, repl)); */

    while(1)
    {
        printf(">> "); fflush(stdout);
        print(stdout, eval(read()));
        puts("\n"); fflush(stdout);
    }

    return 0;
}

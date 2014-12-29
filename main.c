/* phi-lisp interpreter (prototype) : 2014 zk_phi */

#include "philisp.h"
#include "subr.h"

#include <stdio.h>

#if DEBUG
lobj local_env, global_env;
lobj eval(lobj, lobj);
void print(FILE*, lobj);
lobj read();
int main(void)
{
    lobj saved_global_env;

    subr_initialize();

    saved_global_env = global_env;

    /* use pseudo-repl to reduce debug output. */
    while(1)
    {
        printf(">> "); fflush(stdout);
        local_env = NIL, global_env = saved_global_env;
        print(stdout, eval(read(), NIL));
        puts("\n"); fflush(stdout);
    }
}
#endif

#if !DEBUG
lobj eval(lobj, lobj);
int main(void)
{
    lobj repl =
        list(2, function(512+1, list(1, intern("repl")), list(1, intern("repl"))),
             function((~0) << 8, symbol(),
                      list(4, intern("repl"),
                           list(2, intern("puts"), string(">> ")),
                           list(2, intern("print"),
                                list(2, intern("eval"),
                                     list(1, intern("read")))),
                           list(2, intern("puts"), string("\n\n")))));
    /* = ((fn (repl) (repl)) */
    /*    (fn (gensym) (repl (puts ">> ") (print (eval (read))) (puts "\n\n")))) */

    subr_initialize();
    eval(repl, NIL);
}
#endif

/* phi-lisp interpreter (prototype) : 2014 zk_phi */

#include "structures.h"
#include "core.h"
#include "subr.h"

#include <stdio.h>

#if DEBUG
lobj local_env, global_env;
lobj eval(lobj, lobj);
void print(FILE*, lobj);
lobj read();
int main(void)
{
    lobj saved_env;

    core_initialize();
    subr_initialize();

    /* use pseudo-repl to reduce debug output. */
    saved_env = save_current_env(1);
    while(1)
    {
        printf(">> "); fflush(stdout);
        restore_current_env(saved_env);
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

    core_initialize();
    subr_initialize();
    eval(repl, NIL);
}
#endif

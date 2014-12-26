/* phi-lisp interpreter (prototype) : 2014 zk_phi */

/* integerとかdoubleがオブジェクトなのは気持ちよくない。GC入れるならそ
 * れぞれのオブジェクトの大きさもなるべく統一したい。 */

/* subrのclosureは取れなくて良い？ (subr内でシンボル参照をしている場合
 * は挙動が変わる可能性がある…まずないだろうけど) */

#include "phi.h"
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

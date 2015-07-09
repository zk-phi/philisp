#include "philisp.h"

#include <math.h>

/* *FIXME* 型エラーどうやって通知するねん */

DEFSUBR(math_sin, E, _)(lobj args)
{
    if(integerp(car(args)))
        return floating(sin((double)integer_value(car(args))));
    else
        return floating(sin(floating_value(car(args))));
}

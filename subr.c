#include "phi.h"
#include "subr.h"

#include <stdlib.h>
#include <ctype.h>
#include <dlfcn.h>

#define unused(var) (void)(var) /* suppress "unused variable" warning */

lobj eval(lobj, lobj);

/* + ENVIRONMENT    ---------------- */

/* protects = unwind hooks added by unwind-protect */
lobj current_env = NIL, callstack = NIL, unwind_protects = NIL;
FILE *current_in, *current_out, *current_err;

/* push boundary to current environ */
void env_boundary() { current_env = cons(NIL, current_env); }

/* search for a binding of O. if LIMIT is non-0, search only before the
 * last boundry. returns binding, or NIL if unbound. */
lobj binding(lobj o)
{
    lobj env = current_env;

    while(env)
    {
        if(car(env) && car(car(env)) == o)
            return car(env);
        else
            env = cdr(env);
    }

    return NIL;
}

/* if binding of O is found before a boundary, modify the
 * binding. otherwise push a new binding of O to VALUE. */
void bind(lobj o, lobj value)
{
    lobj env = current_env;

    while(env && car(env))
    {
        if(car(car(env)) == o)
        {
            setcdr(car(env), value);
            return;
        }

        else
            env = cdr(env);
    }

    WITH_GC_PROTECTION()
        current_env = cons(cons(o, value), current_env);
}

/* + UTILITIES      ---------------- */

/* make an array from a list */
lobj list_array(lobj lst)
{
    lobj pair = lst, o, *ptr;
    unsigned len = 0;

    while(pair)
        len++, pair = cdr(pair);

    o = make_array(len, NIL);
    for(ptr = array_ptr(o); len--; ptr++, lst = cdr(lst))
        *ptr = car(lst);

    return o;
}

/* *TODO* IMPLEMENT STACK DUMPER, ERROR HANDLER */

void type_error(char* name, unsigned ix, char* expected)
{
    fprintf(current_err,
            "TYPE ERROR: %d-th arg for %s is not a %s\n",
            ix, name, expected);
    exit(1);
}

void lisp_error(char* msg)
{
    fprintf(current_err, "ERROR: %s\n", msg);
    exit(1);
}

void internal_error(char* msg)
{
    fprintf(current_err, "INTERNAL ERROR: %s\n", msg);
    exit(1);
}

/* + NIL            ---------------- */

/* (nil? O) => an unspecified non-() value if O is (), or ()
 * otherwise. */
DEFSUBR(subr_nilp, E, _)(lobj args) { return car(args) ? NIL : symbol(); }

/* + SYMBOL         ---------------- */

/* (symbol? O) => O if O is a symbol, or () otherwise. */
DEFSUBR(subr_symbolp, E, _)(lobj args) { return symbolp(car(args)) ? car(args) : NIL; }

/* (gensym) => an uninterned symbol. */
DEFSUBR(subr_gensym, _, _)(lobj args) { unused(args); return symbol(); }

/* (intern NAME) => a symbol associated with NAME. */
DEFSUBR(subr_intern, E, _)(lobj args)
{
    if(!stringp(car(args)))
        type_error("subr \"intern\"", 0, "string");

    return intern(string_ptr(car(args)));
}

/* + ENVIRON        ---------------- */

/* (bind! O1 [O2]) => bind O1 to object O2 in the innermost scope and
 * return O2. if O2 is omitted, bind O1 to (). */
DEFSUBR(subr_bind, E, E)(lobj args)
{
    lobj o2 = cdr(args) ? car(cdr(args)) : NIL;
    bind(car(args), o2);
    return o2;
}

/* (set! O1 O2) => update binding of O1 to O2 and return O2. error if
 * O1 is unbound. */
void print(FILE*, lobj);
DEFSUBR(subr_set, E E, E)(lobj args)
{
    lobj pair = binding(car(args));

    if(pair)
        setcdr(pair, car(cdr(args)));
    else
        lisp_error("cannot set unbound symbol.");

    return car(cdr(args));
}

/* (bound-value O [ERRORBACK]) => object which O is bound to. if O is
 * unbound, call ERRORBACK with error message, or error if ERRORBACK
 * is omitted. */
DEFSUBR(subr_bound_value, E, E)(lobj args)
{
    lobj pair = binding(car(args));

    if(pair)
        return cdr(pair);

    else if(cdr(args))          /* *FIXME* OPTIMIZE TAIL-CALL */
    {
        lobj o;

        WITH_GC_PROTECTION()
            o = cons(car(cdr(args)),
                     cons(string("reference to unbound symbol."), NIL));

        return eval(o, NIL);
    }

    else
        lisp_error("reference to unbound symbol.");
}

/* + CHAR           ---------------- */

/* (character? O) => O if O is a character, or () otherwise. */
DEFSUBR(subr_charp, E, _)(lobj args) { return characterp(car(args)) ? car(args) : NIL; }

/* (char->int CHAR) => ASCII encode CHAR. */
DEFSUBR(subr_char_to_int, E, _)(lobj args)
{
    if(!characterp(car(args)))
        type_error("subr \"char->int\"", 0, "character");

    return integer(character_value(car(args)));
}

/* (int->char N) => ASCII decode N. */
DEFSUBR(subr_int_to_char, E, _)(lobj args)
{
    if(!integerp(car(args)))
        type_error("subr \"int->char\"", 0, "integer");

    return character((char)integer_value(car(args)));
}

/* + INT            ---------------- */

/* (integer? O) => O if O is an integer, or () otherwise. */
DEFSUBR(subr_integerp, E, _)(lobj args) { return integerp(car(args)) ? car(args) : NIL; }

/* + FLOAT          ---------------- */

/* (float? O) => O if O is a float, or () otherwise. */
DEFSUBR(subr_floatp, E, _)(lobj args) { return floatingp(car(args)) ? car(args) : NIL; }

/* + ARITHMETIC     ---------------- */

int all_integerp(lobj lst)
{
    while(lst)
    {
        if(!integerp(car(lst)))
            return 0;
        else
            lst = cdr(lst);
    }

    return 1;
}

/* (mod INT1 INT2) => return (INT1 % INT2). */
DEFSUBR(subr_mod, E E, _)(lobj args)
{
    if(!integerp(car(args)))
        type_error("subr \"mod\"", 0, "integer");

    if(!integerp(car(cdr(args))))
        type_error("subr \"mod\"", 1, "integer");

    return integer(integer_value(car(args)) % integer_value(car(cdr(args))));
}

/* (/ INT1 INT2 ...) => return (INT1 / INT2 / ...). */
DEFSUBR(subr_quot, E, E)(lobj args)
{
    int val;
    unsigned i;

    if(!integerp(car(args)))
        type_error("subr \"/\"", 0, "integer");

    val = integer_value(car(args));

    args = cdr(args), i = 1;
    while(args)
    {
        if(!integerp(car(args)))
            type_error("subr \"/\"", i, "integer");

        val /= integer_value(car(args));
        args = cdr(args), i++;
    }

    return integer(val);
}

/* (round NUM) => the largest integer no greater than NUM. */
DEFSUBR(subr_round, E, _)(lobj args)
{
    if(integerp(car(args)))
        return car(args);
    else if(floatingp(car(args)))
        return integer((int)floating_value(car(args)));
    else
        type_error("subr \"round\"", 0, "number");
}

/* (+ NUM1 ...) => sum of NUM1, NUM2, ... . result is an integer
 * iff NUM1, NUM2, ... are all integer. */
DEFSUBR(subr_add, _, E)(lobj args)
{
    if(all_integerp(args))
    {
        int sum = 0;

        while(args)
        {
            sum += integer_value(car(args));
            args = cdr(args);
        }

        return integer(sum);
    }

    else
    {
        double sum = 0;
        unsigned ix = 0;

        while(args)
        {
            if(integerp(car(args)))
                sum += integer_value(car(args));
            else if(floatingp(car(args)))
                sum += floating_value(car(args));
            else
                type_error("subr \"+\"", ix, "number");
            ix++;
        }

        return floating(sum);
    }
}

/* (* NUM1 ...) => product of NUM1, NUM2, ... . result is an
 * integer iff NUM1, NUM2, ... are all integer. */
DEFSUBR(subr_mult, _, E)(lobj args)
{
    if(all_integerp(args))
    {
        int prod = 1;

        while(args)
        {
            prod *= integer_value(car(args));
            args = cdr(args);
        }

        return integer(prod);
    }

    else
    {
        double prod = 1.0;
        unsigned ix = 0;

        while(args)
        {
            if(integerp(car(args)))
                prod *= integer_value(car(args));
            else if(floatingp(car(args)))
                prod *= floating_value(car(args));
            else
                type_error("subr \"*\"", ix, "number");
            ix++;
        }

        return floating(prod);
    }
}

/* (- NUM1 NUM2 ...) => negate NUM1 or subtract NUM2 ... from
 * NUM1. result is an integer iff NUM1, NUM2 ... are all integers. */
DEFSUBR(subr_sub, E, E)(lobj args)
{
    if(cdr(args))               /* more than 1 args */
    {
        if(all_integerp(args))
        {
            int res = integer_value(car(args));

            args = cdr(args);
            while(args)
            {
                res -= integer_value(car(args));
                args = cdr(args);
            }

            return integer(res);
        }

        else
        {
            double res;
            unsigned ix = 1;

            if(integerp(car(args)))
                res = integer_value(car(args));
            else if(floatingp(car(args)))
                res = floating_value(car(args));
            else
                type_error("subr \"-\"", 0, "number");

            args = cdr(args);
            while(args)
            {
                if(integerp(car(args)))
                    res -= integer_value(car(args));
                else if(floatingp(car(args)))
                    res -= floating_value(car(args));
                else
                    type_error("subr \"-\"", ix, "number");
                ix++;
            }

            return floating(res);
        }
    }

    else                        /* only 1 arg */
    {
        if(integerp(car(args)))
            return integer(-integer_value(car(args)));
        else if(floatingp(car(args)))
            return floating(-floating_value(car(args)));
        else
            type_error("subr \"-\"", 0, "number");
    }
}

/* (div NUM1 NUM2 ...) => invert NUM1 or divide NUM1 with NUM2
 * ... . result is always a float. */
DEFSUBR(subr_div, E, E)(lobj args)
{
    if(cdr(args))               /* more than 1 args */
    {
        double res;
        unsigned ix = 1;

        if(integerp(car(args)))
            res = integer_value(car(args));
        else if(floatingp(car(args)))
            res = floating_value(car(args));
        else
            type_error("subr \"/\"", 0, "number");

        args = cdr(args);
        while(args)
        {
            if(integerp(car(args)))
                res /= integer_value(car(args));
            else if(floatingp(car(args)))
                res /= floating_value(car(args));
            else
                type_error("subr \"/\"", ix, "number");
            ix++;
        }

        return floating(res);
    }

    else                        /* only 1 arg */
    {
        if(integerp(car(args)))
            return floating(1.0 / integer_value(car(args)));
        else if(floatingp(car(args)))
            return floating(1.0 / floating_value(car(args)));
        else
            type_error("subr \"/\"", 0, "number");
    }
}

#define DEFINE_ORD_SUBR(name, cmpop)                                    \
    DEFSUBR(name, _, E)(lobj args)                                      \
    {                                                                   \
        if(!args)                /* no args */                          \
            return symbol();                                            \
        else                                                            \
        {                                                               \
            double num1, num2;                                          \
            unsigned ix = 1;                                            \
            lobj last;                                                  \
                                                                        \
            if(integerp(car(args)))                                     \
                num1 = integer_value(car(args));                        \
            else if(floatingp(car(args)))                               \
                num1 = floating_value(car(args));                       \
            else                                                        \
                type_error("subr \"" #name "\"", 0, "number");          \
                                                                        \
            args = cdr(args);                                           \
            while(args)                                                 \
            {                                                           \
                if(integerp(car(args)))                                 \
                    num2 = integer_value(car(args));                    \
                else if(floatingp(car(args)))                           \
                    num2 = floating_value(car(args));                   \
                else                                                    \
                    type_error("subr \"" #name "\"", ix, "number");     \
                                                                        \
                if(!(num1 cmpop num2)) return NIL;                      \
                                                                        \
                num1 = num2, ix++, last = args, args = cdr(args);       \
            }                                                           \
                                                                        \
            return car(last);                                           \
        }                                                               \
    }                                                                   \

/* (<= NUM1 ...) => last number if NUM1 ... is weakly increasing, or
 * () otherwise. if no numbers are given, return an unspecified non-()
 * value. */
DEFINE_ORD_SUBR(subr_le, <=)

/* (< NUM1 ...) => last number if NUM1 ... is strongly increasing, or
 * () otherwise. if no numbers are given, return an unspecified non-()
 * value. */
DEFINE_ORD_SUBR(subr_lt, <)

/* (>= NUM1 ...) => last number if NUM1 ... is weakly decreasing, or
 * () otherwise. if no numbers are given, return an unspecified non-()
 * value. */
DEFINE_ORD_SUBR(subr_ge, >=)

/* (> NUM1 ...) => last number if NUM1 ... is strongly decreasing, or
 * () otherwise. if no numbers are given, return an unspecified non-()
 * value. */
DEFINE_ORD_SUBR(subr_gt, >)

/* + STREAM         ---------------- */

/* (stream? O) => O if O is a stream, or () otherwise. */
DEFSUBR(subr_streamp, E, _)(lobj args) { return streamp(car(args)) ? car(args) : NIL; }

/* (input-port) => current input port, which defaults to stdin. */
DEFSUBR(subr_input_port, _, _)(lobj args) { unused(args); return stream(current_in); }

/* (output-port) => current output port, which defaults to stdout. */
DEFSUBR(subr_output_port, _, _)(lobj args) { unused(args); return stream(current_out); }

/* (error-port) => current error port, which defaults to stderr. */
DEFSUBR(subr_error_port, _, _)(lobj args) { unused(args); return stream(current_err); }

/* (set-ports [ISTREAM OSTREAM ESTREAM]) => change input port to
 * ISTREAM (resp. output port, error port). some of arguments can be
 * omitted or (), which represents "no-change". (return value is
 * unspecified) */
DEFSUBR(subr_set_ports, _, E)(lobj args)
{
    if(args)
    {
        if(car(args))
        {
            if(!streamp(car(args)))
                type_error("subr \"set-ports\"", 0, "stream");
            current_in = stream_value(car(args));
        }

        if((args = cdr(args)))
        {
            if(car(args))
            {
                if(!streamp(car(args)))
                    type_error("subr \"set-ports\"", 1, "stream");
                current_out = stream_value(car(args));
            }

            if((args = cdr(args)))
                if(car(args))
                {
                    if(!streamp(car(args)))
                        type_error("subr \"set-ports\"", 2, "stream");
                    current_err = stream_value(car(args));
                }
        }
    }

    return NIL;
}

/* (getc [ERRORBACK]) => get a character from input port. on failure,
 * ERRORBACK is called with error message, or error if ERRORBACK is
 * omitted. */
DEFSUBR(subr_getc, _, E)(lobj args)
{
    int val;
    unused(args);

    if((val = getc(current_in)) != EOF)
        return character(val);

    else if(args)               /* *FIXME* OPTIMIZE TAIL-CALL */
    {
        lobj o;

        WITH_GC_PROTECTION()
            o = cons(car(args),
                     cons(string("failed to get character."), NIL));

        return eval(o, NIL);
    }

    else
        lisp_error("failed to get character.");
}

/* (putc CHAR [ERRORBACK]) => write CHAR to output port and return
 * CHAR. on failure, ERRORBACK is called with error message, or error
 * if ERRORBACK is omitted. */
DEFSUBR(subr_putc, E, E)(lobj args)
{
    if(!characterp(car(args)))
        type_error("subr \"putc\"", 0, "character");

    if(putc(character_value(car(args)), current_out) == EOF)
    {
        if(cdr(args))           /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)),
                         cons(string("failed to put character"), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error("failed to put character.");
    }

    return car(args);
}

/* (ungetc CHAR [ERRORBACK]) => unget CHAR from input stream and
 * return CHAR. when this subr is called multiple times without
 * re-getting the ungot char, behavior is not guaranteed. on failure,
 * ERRORBACK is called with error message, or error if ERRORBACK is
 * omitted. */
DEFSUBR(subr_ungetc, E, E)(lobj args)
{
    if(!characterp(car(args)))
        type_error("subr \"ungetc\"", 0, "character");

    if(ungetc(character_value(car(args)), current_in) == EOF)
    {
        if(cdr(args))           /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)),
                         cons(string("failed to unget character."), NIL));

            return eval(o, NIL);
        }

        lisp_error("failed to unget character.");
    }

    return car(args);
}

/* (open FILE [WRITABLE APPEND BINARY ERRORBACK]) => open a stream for
 * FILE. if WRITABLE is omitted or (), open FILE in read-only
 * mode. (resp. APPEND, BINARY) */
DEFSUBR(subr_open, E, E)(lobj args)
{
    FILE* f;
    char *filename, mode[4];
    unsigned ix;

    /* prepare FILENAME */
    if(!stringp(car(args)))
        type_error("subr \"open\"", 0, "string");
    filename = string_ptr(car(args));

    /* prepare MODE */
    ix = 1, mode[0] = 'r', args = cdr(args);
    if(args)
    {
        if(car(args)) mode[ix] = 'w', ix++;
        if((args = cdr(args)))
        {
            if(car(args)) mode[ix] = '+', ix++;
            if((args = cdr(args)))
                if(car(args)) mode[ix] = 'b', ix++;
        }
    }
    mode[ix] = '\0';

    /* call FOPEN */
    if(!(f = fopen(filename, mode)))
    {
        if(cdr(args))           /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)), cons(string("failed to open file"), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error("failed to open file.");
    }

    return stream(f);
}

/* (close! STREAM [ERRORBACK]) => close STREAM (return value is
 * unspecified). on failure, ERRORBACK is called with error message,
 * or error if ERRORBACK is omitted. */
DEFSUBR(subr_close, E, E)(lobj args)
{
    if(!streamp(car(args)))
        type_error("subr \"close!\"", 0, "stream");

    if(fclose(stream_value(car(args))) == EOF)
    {
        if(cdr(args))           /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)), cons(string("failed to close stream."), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error("failed to close stream.");
    }

    return NIL;
}

/* + CONS           ---------------- */

/* (cons? O) => O if O is a pair, or () otherwise. */
DEFSUBR(subr_consp, E, _)(lobj args) { return consp(car(args)) ? car(args) : NIL;  }

/* (cons O1 O2) => pair of O1 and O2. */
DEFSUBR(subr_cons, E E, _)(lobj args) { return cons(car(args), car(cdr(args))); }

/* (car PAIR) => CAR part of PAIR. if PAIR is (), return (). */
DEFSUBR(subr_car, E, _)(lobj args)
{
    if(!car(args))
        return NIL;
    else if(consp(car(args)))
        return car(car(args));
    else
        type_error("subr \"car\"", 0, "cons nor ()");
}

/* (cdr PAIR) => CDR part of PAIR. if PAIR is (), return (). */
DEFSUBR(subr_cdr, E, _)(lobj args)
{
    if(!car(args))
        return NIL;
    else if(consp(car(args)))
        return cdr(car(args));
    else
        type_error("subr \"cdr\"", 0, "cons nor ()");
}

/* (setcar! PAIR NEWCAR) => set CAR part of PAIR to NEWCAR. return NEWCAR. */
DEFSUBR(subr_setcar, E E, _)(lobj args)
{
    if(!consp(car(args)))
        type_error("subr \"setcar!\"", 0, "cons");
    setcar(car(args), car(cdr(args)));
    return car(cdr(args));
}

/* (setcdr! PAIR NEWCDR) => set CDR part of PAIR to NEWCDR. return NEWCDR. */
DEFSUBR(subr_setcdr, E E, _)(lobj args)
{
    if(!consp(car(args)))
        type_error("subr \"setcdr!\"", 0, "cons");
    setcdr(car(args), car(cdr(args)));
    return car(cdr(args));
}

/* + ARRAY          ---------------- */

/* (array? O) => O if O is an array, or () otherwise. */
DEFSUBR(subr_arrayp, E, _)(lobj args)
{
    return arrayp(car(args)) || stringp(car(args)) ? car(args) : NIL;
}

/* (make-array LENGTH [INIT]) => make an array of LENGTH slots which
 * defaults to INIT. if INIT is omitted, initialize with ()
 * instead. */
DEFSUBR(subr_make_array, E, E)(lobj args)
{
    int len;
    lobj init = cdr(args) ? car(cdr(args)) : NIL;

    if(!integerp(car(args)))
        type_error("subr \"make-array\"", 0, "positive integer");

    if((len = integer_value(car(args))) < 0)
        type_error("subr \"make-array\"", 0, "positive integer");

    if(characterp(init))
        return make_string(len, character_value(init));
    else
        return make_array(len, init);
}

/* (aref ARRAY N) => N-th element of ARRAY. error if N is negative or
 * greater than the length of ARRAY. */
DEFSUBR(subr_aref, E E, _)(lobj args)
{
    int ix;

    if(!integerp(car(cdr(args))))
        type_error("subr \"aref\"", 1, "positive integer");

    if((ix = integer_value(car(cdr(args)))) < 0)
        type_error("subr \"aref\"", 1, "positive integer");

    if(arrayp(car(args)))
    {
        if((unsigned)ix >= array_length(car(args)))
            lisp_error("array boundary error");

        return (array_ptr(car(args)))[ix];
    }

    else if(stringp(car(args)))
    {
        if((unsigned)ix >= string_length(car(args)))
            lisp_error("array boundary error");

        return character((string_ptr(car(args)))[ix]);
    }

    else
        type_error("subr \"aref\"", 0, "array");
}

/* (aset! ARRAY N O) => set N-th element of ARRAY to O and return
 * O. error if O is negative or greater than the length of ARRAY. */
DEFSUBR(subr_aset, E E, _)(lobj args)
{
    int ix;

    if(stringp(car(args)) && !characterp(car(cdr(args))))
        string_to_array(car(args));

    if(!integerp(car(cdr(args))))
        type_error("subr \"aset!\"", 1, "positive integer");
    if((ix = integer_value(car(cdr(args)))) < 0)
        type_error("subr \"aset!\"", 1, "positive integer");

    if(arrayp(car(args)))
    {
        if((unsigned)ix >= array_length(car(args)))
            lisp_error("array boundary error");

        return (array_ptr(car(args)))[ix] = car(cdr(args));
    }

    else if(stringp(car(args)))
    {
        if((unsigned)ix >= string_length(car(args)))
            lisp_error("array boundary error");

        (string_ptr(car(args)))[ix] = character_value(car(cdr(args)));

        return car(cdr(args));
    }

    else
        type_error("subr \"aset!\"", 0, "array");
}

/* (string? O) => O if O is a char-array, or () otherwise. */
DEFSUBR(subr_stringp, E, _)(lobj args) { return stringp(car(args)) ? car(args) : NIL; }

/* + FUNCTION       ---------------- */

/* (function? O) => O iff O is a function, or () otherwise. */
DEFSUBR(subr_functionp, E, _)(lobj args) { return functionp(car(args)) ? car(args) : NIL; }

/* (fn ,FORMALS ,EXPR) => a function. */
DEFSUBR(subr_fn, Q Q, _)(lobj args)
{
    lobj formals = car(args);

    if(!formals)                /* 0 */
        return function(0, NIL, car(cdr(args)));

    else if(!consp(formals))    /* 0+ (eval) */
        return function(~0 << 8, formals, car(cdr(args)));

    else if(car(formals) == intern("eval")) /* 0+ (quote) */
    {
        lobj s = car(cdr(formals));

        if(!symbolp(s))
            lisp_error("invalid syntax in subr \"fn\".");

        return function(256, s, car(cdr(args)));
    }

    WITH_GC_PROTECTION()    /* more than 1 */
    {
        lobj head, tail;
        unsigned char len;
        int pattern, mask;

        if(!consp(car(formals))) /* (x ...) */
            head = tail = cons(car(formals), NIL), pattern = 1, len = 1;
        else if(car(car(formals)) == intern("eval") /* ((eval x) ...) */
                && symbolp(car(cdr(car(formals)))))
            head = tail = cons(car(cdr(car(formals))), NIL), pattern = 0, len = 1;
        else
            lisp_error("invalid syntax in subr \"fn\".");

        mask = 2, formals = cdr(formals);
        while(formals)
        {
            if(!consp(formals)) /* x */
            {
                setcdr(tail, formals);
                return function((~0 << (9 + len)) | (pattern << 9) | 256 | len,
                                head, car(cdr(args)));
            }
            else if(car(formals) == intern("eval")) /* (eval x) */
            {
                lobj s = car(cdr(formals));

                if(!symbolp(s))
                    lisp_error("invalid syntax in subr \"fn\".");

                setcdr(tail, s);

                return function(pattern << 9 | 256 | len, head, car(cdr(args)));
            }
            else if(!consp(car(formals))) /* (x ...) */
            {
                setcdr(tail, cons(car(formals), NIL));
                tail = cdr(tail), pattern = pattern | (mask & 1), len++;
                mask <<= 1, formals = cdr(formals);
            }
            else if(car(car(formals)) == intern("eval")       /* ((eval x) ...) */
                    && symbolp(car(cdr(car(formals)))))
            {
                setcdr(tail, cons(car(cdr(car(formals))), NIL));
                tail = cdr(tail), len++;
                formals = cdr(formals);
            }
            else
                lisp_error("invalid syntax in subr \"fn\".");
        }

        return function(pattern << 9 | len, head, car(cdr(args)));
    }
}

/* + CLOSURE        ---------------- */

/* (closure? O) => O iff O is a function, or () otherwise. */
DEFSUBR(subr_closurep, E, _)(lobj args) { return closurep(car(args)) ? car(args) : NIL; }

/* (closure FN) => make a closure of function FN. */
DEFSUBR(subr_closure, E, _)(lobj args)
{
    if(!functionp(car(args)))
        type_error("subr \"closure\"", 0, "function");
    return closure(car(args), current_env);
}

/* + C-FUNCTION     ---------------- */

/* (subr? O) => O iff O is a subr (a compiled function), or ()
 * otherwise. */
DEFSUBR(subr_subrp, E, _)(lobj args) { return subrp(car(args)) ? car(args) : NIL; }

/* (dlsubr FILENAME SUBRNAME [ERRORBACK]) => load SUBRNAME from
 * FILENAME. on failure, ERRORBACK is called with error message, or
 * error if ERRORBACK is omitted. */
DEFSUBR(subr_dlsubr, E, E)(lobj args)
{
    void* h;
    lsubr *ptr;

    if(!stringp(car(args)))
        type_error("subr \"dlsubr\"", 0, "string");

    if(!(h = dlopen(string_ptr(car(args)), RTLD_LAZY)))
    {
        if(cdr(cdr(args)))      /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(cdr(args))),
                         cons(string("filed to load shared object."), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error("failed to load shared object.");
    }

    if(!stringp(car(cdr(args))))
        type_error("subr \"dlsubr\"", 1, "string");

    if(!(ptr = dlsym(h, string_ptr(car(cdr(args))))))
    {
        if(cdr(cdr(args)))      /* *FIXME* OPTIMIZE TAIL-CALL */
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(cdr(args))),
                         cons(string("filed to find symbol from shared object."), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error("failed to find symbol from shared object.");
    }

    return subr(*ptr);
}

/* + CONTINUATION   ---------------- */

/* (continuation? O) => O iff O is a continuation object, or () otherwise. */
DEFSUBR(subr_continuationp, E, _)(lobj args)
{
    return continuationp(car(args)) ? car(args) : NIL;
}

/* + EQUALITY       ---------------- */

/* (eq O1 ...) => an unspecified non-() value if O1 ... are all the
 * same object, or () otherwise. */
DEFSUBR(subr_eq, _, E)(lobj args)
{
    if(!args)                   /* no args */
        return symbol();
    else
    {
        lobj last = car(args);

        args = cdr(args);
        while(args)
        {
            if(last != car(args)) return NIL;
            last = car(args), args = cdr(args);
        }

        return car(last);
    }
}

/* (char= CH1 ...) => last char if CH1 ... are all equal as chars, or
 * () otherwise. if no characters are given, return an unspecified
 * non-() value. */
DEFSUBR(subr_char_eq, _, E)(lobj args)
{
    if(!args)                   /* no args */
        return symbol();
    else
    {
        char ch1, ch2;
        unsigned ix = 1;
        lobj last;

        if(characterp(car(args)))
            ch1 = character_value(car(args));
        else
            type_error("subr \"char=\"", 0, "character");

        args = cdr(args);
        while(args)
        {
            if(characterp(car(args)))
                ch2 = character_value(car(args));
            else
                type_error("subr \"char=\"", ix, "character");

            if(ch1 != ch2) return NIL;

            ch1 = ch2, ix++, last = args, args = cdr(args);
        }

        return car(last);
    }
}

/* (= NUM1 ...) => last number if NUM1 ... are all equal as numbers,
 * or () otherwise. if no numbers are given, return an unspecified
 * non-() value. */
DEFINE_ORD_SUBR(subr_num_eq, ==)

/* + UNPARSER       ---------------- */

void put_literal_char(FILE* stream, char ch)
{
    switch(ch)
    {
      case '\a': fprintf(stream, "\\a"); break;
      case '\b': fprintf(stream, "\\b"); break;
      case '\f': fprintf(stream, "\\f"); break;
      case '\n': fprintf(stream, "\\n"); break;
      case '\r': fprintf(stream, "\\r"); break;
      case '\t': fprintf(stream, "\\t"); break;
      case '\v': fprintf(stream, "\\v"); break;
      case '\\': fprintf(stream, "\\\\"); break;
      case '\"': fprintf(stream, "\\\""); break;
      default:
        if(isprint((int)ch))
            putc(ch, stream);
        else
            fprintf(stream, "\\x%2x", ch);
    }
}

void print(FILE* stream, lobj o)
{
    if(!o)
        fprintf(stream, "()");

    else if (symbolp(o))
    {
        static char buf[SYMBOL_NAME_MAX + 1];

        if(!rintern(o, buf, SYMBOL_NAME_MAX + 1))
            fprintf(stream, buf);
        else
            fprintf(stream, "#<symbol %p>", (void*)o);
    }

    else if(characterp(o))
    {
        putc('?', stream);
        put_literal_char(stream, character_value(o));
    }

    else if(integerp(o))
        fprintf(stream, "%d", integer_value(o));

    else if(floatingp(o))
        fprintf(stream, "%f", floating_value(o));

    else if(streamp(o))
        fprintf(stream, "#<stream %p>", (void*)o);

    else if(consp(o))
    {
        putc('(', stream);
        while(1)
        {
            if(!cdr(o))
            {
                print(stream, car(o));
                putc(')', stream);
                break;
            }

            else if(!consp(cdr(o)))
            {
                print(stream, car(o));
                fprintf(stream, " . ");
                print(stream, cdr(o));
                putc(')', stream);
                break;
            }

            else
            {
                print(stream, car(o));
                putc(' ', stream);
                o = cdr(o);
            }
        }
    }

    else if(stringp(o))
    {
        unsigned len = string_length(o);
        char *ptr = string_ptr(o);

        putc('\"', stream);

        for(; len--; ptr++)
            put_literal_char(stream, *ptr);

        putc('\"', stream);
    }

    else if(arrayp(o))
    {
        lobj *arr = array_ptr(o);
        unsigned len = array_length(o);

        putc('[', stream);
        if(len >= 1)
        {
            while(len-- > 1)
            {
                print(stream, *(arr++));
                putc(' ', stream);
            }
            print(stream, *arr);
        }
        putc(']', stream);
    }

    else if(functionp(o))
    {
        lobj expr = function_expr(o);
        pargs args = function_args(o);

        fprintf(stream, "#<func:%d%s ", args & 255, args & 256 ? "+" : "");

        if(consp(expr) && !consp(car(expr)) && !arrayp(car(expr)))
        {
            fprintf(stream, "(");
            print(stream, car(expr));
            fprintf(stream, " ...)");
        }
        else
            fprintf(stream, "%p", (void*)o);

        putc('>', stream);
    }

    else if(closurep(o))
    {
        pargs args = function_args(closure_function(o));
        fprintf(stream, "#<closure:%d%s %p>",
                args & 255, args & 256 ? "+" : "", (void*)o);
    }

    else if(subrp(o))
    {
        pargs args = subr_args(o);
        fprintf(stream, "#<subr:%d%s %s>",
                args & 255, args & 256 ? "+" : "", subr_description(o));
    }

    else if(continuationp(o))
        fprintf(stream, "#<cont:1 %p>", (void*)o);

    else if(papplp(o))
    {
        fprintf(stream, "#<func:(pa/");
        print(stream, pappl_function(o));
        fprintf(stream, ")>");
    }

    else
        fprintf(stream, "#<broken object?>");
}

/* (print O) => print string representation of object O to output port
 * and return O. */
DEFSUBR(subr_print, E, _)(lobj args)
{
    print(current_out, car(args));
    return car(args);
}

/* + PARSER         ---------------- */

int read_char()
{
    int ch;
    while(isspace((ch = getc(current_in))));
    return ch;
}

/* like getchar but aware of escape-sequence. if the char is endchar,
 * return -2. if failed to parse, return -3. */
int get_literal_char(int endchar)
{
    int ch = getc(current_in);

    if(ch == endchar)
        return -2;
    else if(ch != '\\')
        return ch;
    else
    {
        ch = getc(current_in);

        /* octal constant */
        if('0' <= ch && ch <= '8')
        {
            unsigned char v = ch - '0';
            char i;

            for(i = 0; i < 3; i++)
            {
                ch = getc(current_in);
                if('0' <= ch && ch <= '8')
                    v = v * 8 + (ch - '0');
                else
                    ungetc(ch, current_in);
            }

            return v;
        }

        switch(ch)
        {
          case 'a':  return '\a';
          case 'b':  return '\b';
          case 'f':  return '\f';
          case 'n':  return '\n';
          case 'r':  return '\r';
          case 't':  return '\t';
          case 'v':  return '\v';
          case '\\': return '\\';
          /* case '?':  return '?'; /\* useless (no trigraphs in "phi") *\/ */
          /* case '\'': return '\''; /\* useless (' is not a char-literal) *\/ */
          case '\"': return '\"';

          case 'x':             /* hexadecimal constant */
            {
                unsigned char v;
                char i;

                for(i = 0; i < 2; i++)
                {
                    ch = getc(current_in);
                    if('0' <= ch && ch <= '9')
                        v = v * 8 + (ch - '0');
                    else if('a' <= ch && ch <= 'f')
                        v = v * 8 + (ch - 'a');
                    else if('A' <= ch && ch <= 'F')
                        v = v * 8 + (ch - 'A');
                    else
                        ungetc(ch, current_in);
                }

                return v;
            }
        }

        return -3;
    }
}

/* *FIXME* "." is parsed as "0.0" */
/* *TODO* implement comment */

/* read an S-expression and return it. if succeeded, last_parse_error
 * == NULL. otherwise last_parse_error == "error message". */
char* last_parse_error;
#define PARSE_ERROR(str) do{ last_parse_error = str; return NIL; }while(0)
lobj read()
{
    int ch;
    unsigned bufptr = 0;
    char buf[SYMBOL_NAME_MAX + 1];

    last_parse_error = NULL;

    switch(ch = read_char())
    {
      case EOF:
        PARSE_ERROR("unexpected EOF where an expression is expected.");

      case ')':
        PARSE_ERROR("too many ')' in expression.");

      case ']':
        PARSE_ERROR("too many ']' in expression.");

      case '\'':                /* quote */
        WITH_GC_PROTECTION()
            return cons(intern("quote"), cons(read(), NIL));

      case ',':                 /* eval */
        WITH_GC_PROTECTION()
            return cons(intern("eval"), cons(read(), NIL));

      case '?':                 /* char */
        ch = get_literal_char(-1);
        if(ch == EOF)
            PARSE_ERROR("unexpected EOF after ?.");
        else if(ch == -3)
            PARSE_ERROR("invalid escape sequence.");
        else
            return character(ch);

      case '(':                 /* list or () */
        if((ch = read_char()) == ')')
            return NIL;
        else
        {
            ungetc(ch, current_in);
            WITH_GC_PROTECTION()
            {
                lobj head = cons(read(), NIL), last = head;

                while((ch = read_char()) != ')')
                {
                    if(ch == EOF)
                        PARSE_ERROR("unexpected EOF in a list.");

                    else if(ch == '.')
                    {
                        setcdr(last, read());
                        if(read_char() != ')')
                            PARSE_ERROR("more than one elements after dot.");
                        return head;
                    }

                    else
                    {
                        ungetc(ch, current_in);
                        setcdr(last, cons(read(), NIL));
                        last = cdr(last);
                    }
                }

                return head;
            }
        }

      case '[':                 /* array */
        if((ch = read_char()) == ']')
            return make_array(0, NIL);
        else
        {
            ungetc(ch, current_in);
            WITH_GC_PROTECTION()
            {
                lobj head = cons(read(), NIL), last = head;

                while((ch = read_char()) != ']')
                {
                    if(ch == EOF)
                        PARSE_ERROR("unexpected EOF in an array literal.");
                    ungetc(ch, current_in);
                    setcdr(last, cons(read(), NIL));
                    last = cdr(last);
                }

                return list_array(head);
            }
        }

      case '\"':                /* string */
        if((ch = getc(current_in)) == '\"')
            return make_string(0, '\0');
        else
        {
            ungetc(ch, current_in);
            WITH_GC_PROTECTION()
            {
                lobj head, last;

                if((ch = get_literal_char(-1)) == EOF)
                    PARSE_ERROR("unexpected EOF in a string literal.");
                else if(ch == -3)
                    PARSE_ERROR("invalid escape sequence.");

                head = last = cons(character(ch), NIL);

                while((ch = get_literal_char('\"')) != -2)
                {
                    if(ch == EOF)
                        PARSE_ERROR("unexpected EOF in a string literal.");
                    else if(ch == -3)
                        PARSE_ERROR("invalid escape sequence.");
                    setcdr(last, cons(character(ch), NIL));
                    last = cdr(last);
                }

                head = list_array(head);
                stringp(head);  /* array -> string */

                return head;
            }
        }

      case '.': case '0': case '1': case '2': case '3': case '4': /* number */
      case '5': case '6': case '7': case '8': case '9':
        {
            unsigned v = 0;

            while('0' <= ch && ch <= '9')
            {
                v = v * 10 + (ch - '0');
                ch = getc(current_in);
            }

            if(ch == '.')
            {
                double vv = 0;

                /* *FIXME* POOR IMPLEMENTATION */
                ch = getc(current_in);
                while('0' <= ch && ch <= '9')
                {
                    vv = vv * 10 + (ch - '0');
                    ch = getc(current_in);
                }
                while(vv > 1) vv /= 10;

                if(ch == 'e')
                {
                    int e = 0;

                    ch = getc(current_in);
                    while('0' <= ch && ch <= '9')
                    {
                        e = e * 10 + (ch - '0');
                        ch = getc(current_in);
                    }

                    for(vv += v; e--; vv *= 10);

                    ungetc(ch, current_in);

                    return floating(vv);
                }

                else
                {
                    ungetc(ch, current_in);
                    return floating(v + vv);
                }
            }

            if(ch == 'e')
            {
                int e = 0;

                ch = getc(current_in);
                while('0' <= ch && ch <= '9')
                {
                    e = e * 10 + (ch - '0');
                    ch = getc(current_in);
                }

                for(; e--; v *= 10);

                ungetc(ch, current_in);
                return integer(v);
            }

            else
            {
                ungetc(ch, current_in);
                return integer(v);
            }
        }

        /* negative number or a symbol */

      case '-': case '+':       /* negative/positive number ? */
        buf[0] = ch;
        bufptr = 1;
        ch = getc(current_in);
        if(ch == '.' || ('0' <= ch && ch <= '9'))
        {
            lobj v;
            ungetc(ch, current_in);
            v = read();

            if(integerp(v))
                return integer(-integer_value(v));
            else if(floatingp(v))
                return floating(-floating_value(v));
            else
                internal_error("unexpected non-number value after '-'.");
        }

        /* vv FALL THROUGH ... vv */

      default:                  /* symbol name */
        while(1)
        {
            if(bufptr == SYMBOL_NAME_MAX)
                internal_error("too long symbol name given.");

            else if(ch == -1 || isspace(ch) || ch == '(' ||
               ch == ')' || ch == '[' || ch == ']' || ch == '\"')
            {
                buf[bufptr] = '\0';
                break;
            }

            else
            {
                buf[bufptr++] = ch;
                ch = getc(current_in);
            }
        }
        ungetc(ch, current_in);
        return intern(buf);
    }
}

/* (read [ERRORBACK]) => read an S-expression from input port. on
 * failure, ERRORBACK is called with error message, or error if
 * ERRORBACK is omitted. */
DEFSUBR(subr_read, _, E)(lobj args)
{
    lobj val;
    unused(args);

    val = read();

    if(last_parse_error)        /* *FIXME* OPTIMIZE TAIL-CALL */
    {
        if(args)
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(args), cons(string(last_parse_error), NIL));

            return eval(o, NIL);
        }

        else
            lisp_error(last_parse_error);
    }

    return val;
}

/* + EVALUATOR      ---------------- */

/* (if COND ,THEN [,ELSE]) => if COND is non-(), evaluate THEN, else
 * evaluate ELSE. if ELSE is omitted, return (). */
DEFSUBR(subr_if, E Q Q, _)(lobj args)
{
    /* ---- THIS IS A DUMMY DEFINITION (subr_eval WILL DO IT) ---- */
    unused(args);
    internal_error("unexpected call to subr \"if\"");
}

/* (evlis PROC EXPRS) => evaluate list of expressions in accordance
 * with evaluation rule of PROC. */
DEFSUBR(subr_evlis, E E, _)(lobj args)
{
    /* ---- THIS IS A DUMMY DEFINITION (subr_eval WILL DO IT) ---- */
    unused(args);
    internal_error("unexpected call to subr \"evlis\"");
}

/* (apply PROC ARGS) => apply ARGS to PROC. */
DEFSUBR(subr_apply, E E, _)(lobj args)
{
    /* ---- THIS IS A DUMMY DEFINITION (subr_eval WILL DO IT) ---- */
    unused(args);
    internal_error("unexpected call to subr \"apply\"");
}

/* (unwind-protect ,BODY ,AFTER) => evaluate BODY and then AFTER. when
 * a continuation is called in BODY, evaluate AFTER before winding the
 * continuation. */
DEFSUBR(subr_unwind_protect, Q Q, _)(lobj args)
{
    /* ---- THIS IS A DUMMY DEFINITION (subr_eval WILL DO IT) ---- */
    unused(args);
    internal_error("unexpected call to subr \"unwind-protect\"");
}

/* (call/cc FUNC) => evaluate BODY and then AFTER. when
 * a continuation is called in BODY, evaluate AFTER before winding the
 * continuation. */
DEFSUBR(subr_call_cc, E, _)(lobj args)
{
    /* ---- THIS IS A DUMMY DEFINITION (subr_eval WILL DO IT) ---- */
    unused(args);
    internal_error("unexpected call to subr \"call/cc\"");
}

int eval_pattern(lobj o)
{
    if(functionp(o))
        return function_args(o) >> 9;
    else if(closurep(o))
        return function_args(closure_function(o)) >> 9;
    else if(subrp(o))
        return subr_args(o) >> 9;
    else if(continuationp(o))
        return 1;
    else if(papplp(o))
        return pappl_eval_pattern(o);
    else
        return ~0;
}

/* *TODO* OPTIMIZE TAIL-CALL */
#define EVALUATION_ERROR(str)                                   \
    do{                                                         \
        if(!errorback)                                          \
            lisp_error(str);                                    \
        else                                                    \
        {                                                       \
            lobj o;                                             \
                                                                \
            WITH_GC_PROTECTION()                                \
                o = cons(errorback, cons(string(str), NIL));    \
                                                                \
            return eval(o, NIL);                                \
        }                                                       \
    }                                                           \
    while(0)

lobj eval(lobj o, lobj errorback)
{
    callstack = NIL;

  eval:                 /* here O is an expression to be evaluated. */

    if(symbolp(o))
    {
        if(!(o = binding(o)))
            EVALUATION_ERROR("reference to unbound symbol.");
        o = cdr(o);
        goto ret;
    }
    else if(consp(o))
    {
        env_boundary();         /* protect "saved_env" from "bind" */
        WITH_GC_PROTECTION() /* stack_frame = [pappl, pending_args, saved_env] */
            callstack = cons(array(3, NIL, cdr(o), current_env), callstack);
        o = car(o);
        goto eval;
    }

  ret:                       /* here O is an object evaluated just now. */

    if(!callstack)           /* nothing more to evaluate */
        return o;
    else
    {
        lobj *ptr = array_ptr(car(callstack));

        /* update pappl */
        if(!ptr[0])             /* pappl is not set */
            ptr[0] = pappl(eval_pattern(o), o);
        else
            pappl_push(ptr[0], o);

        /* restore environ */
        current_env = ptr[2];

        /* then evaluate next "unevaluated" arg or apply all evaluated args */
        if(ptr[1])
        {
            o = car(ptr[1]);
            ptr[1] = cdr(ptr[1]);

            if(pappl_eval_pattern(ptr[0]) & 1)
                goto eval;
            else
                goto ret;
        }
        else
        {
            o = ptr[0];
            callstack = cdr(callstack);
            goto apply;
        }
    }

  apply:                  /* here O is a pappl object to be applied */

    {
        lobj func = pappl_function(o),
             vals = pappl_values(o);
        int num_vals = pappl_num_values(o);

        if(functionp(func))
        {
            pargs num_args = function_args(func);

            if((num_args & 255) < num_vals && !(num_args & 256)) /* too many */
                EVALUATION_ERROR("too many arguments applied to a function.");
            else if(num_vals < (num_args & 255)) /* too few */
                goto ret;
            else                /* okay */
            {
                lobj formals = function_formals(func);

                while(formals)
                {
                    if(consp(formals))
                    {
                        bind(car(formals), car(vals));
                        vals = cdr(vals);
                        formals = cdr(formals);
                    }
                    else
                    {
                        bind(formals, vals);
                        break;
                    }
                }

                o = function_expr(func);
                goto eval;
            }
        }
        else if(closurep(func))
        {
            pargs num_args = function_args(closure_function(func));

            if((num_args & 255) < num_vals && !(num_args & 256)) /* too many */
                EVALUATION_ERROR("too many arguments applied to a closure.");
            else if(num_vals < (num_args & 255)) /* too few */
                goto ret;
            else                /* okay */
            {
                current_env = closure_env(func);
                pappl_set_function(o, closure_function(func));
                goto apply;
            }
        }
        else if(subrp(func))
        {
            pargs num_args = subr_args(func);

            if((num_args & 255) < num_vals && !(num_args & 256)) /* too many */
                EVALUATION_ERROR("too many arguments applied to a subr.");

            else if(num_vals < (num_args & 255)) /* too few */
                goto ret;
            else
            {
                lobj (*fobj)(lobj) = subr_function(func);

                if(fobj == f_subr_if)
                {
                    o = car(vals) ? car(cdr(vals)) : car(cdr(cdr(vals)));
                    goto eval;
                }
                else if(fobj == f_subr_evlis)
                {
                    /* *TODO* IMPLEMENT EVLIS */
                    internal_error("NOT IMPLEMENTED subr \"evlis\".");
                }
                else if(fobj == f_subr_apply)
                {
                    o = pappl(eval_pattern(car(vals)), car(vals));

                    if(!listp(car(cdr(vals))))
                        type_error("subr \"apply\"", 1, "list");

                    for(vals = car(cdr(vals)); vals; vals = cdr(vals))
                        pappl_push(o, car(vals));

                    goto apply;
                }
                else if(fobj == f_subr_unwind_protect)
                {
                    internal_error("NOT IMPLEMENTED subr \"unwind-protect.\"");

                    /* *TODO* IMPLEMENT SUBR "unwind-protect" */

                    /* unwind_protects = cons(car(cdr(vals)), unwind_protects); */
                    /* o = car(vals); */
                    /* goto eval; */
                }
                else if(fobj == f_subr_call_cc)
                {
                    o = pappl(eval_pattern(car(vals)), car(vals));
                    pappl_push(o, continuation(callstack));
                    goto apply;
                }
                else
                {
                    o = (subr_function(func))(vals);
                    goto ret;
                }
            }
        }
        else if(continuationp(func))
        {
            if(1 < num_vals)    /* too many */
                EVALUATION_ERROR("too many arguments applied to a continuation.");
            else if(num_vals < 1) /* too few */
                goto ret;
            else
            {
                if(unwind_protects)
                {
                    /* *TODO* EVAL "unwind_protects" here */
                }

                callstack = continuation_callstack(func);
                o = car(vals);
                goto ret;
            }
        }
        else if(papplp(func))
        {
            /* *FIXME* EFFICIENCY */

            lobj vals2 = pappl_values(func);

            o = pappl(eval_pattern(pappl_function(func)), pappl_function(func));

            while(vals2)
            {
                pappl_push(o, car(vals2));
                vals2 = cdr(vals2);
            }

            while(vals)
            {
                pappl_push(o, car(vals));
                vals = cdr(vals);
            }

            goto apply;
        }
        else if(integerp(func) || floatingp(func))
        {
            if(!vals)           /* (1) = 1 */
            {
                o = func;
                goto ret;
            }
            else if(!cdr(vals)) /* (1 f) = (fn x (apply f 1 x)) */
            {
                o = pappl(eval_pattern(car(vals)), car(vals));
                pappl_push(o, func);
                goto ret;
            }
            else                /* (1 f 2 ...) = ((f 1 2) ...) */
            {
                WITH_GC_PROTECTION()
                    callstack = cons(array(3,
                                           pappl(0, subr(subr_apply)),
                                           cons(cdr(cdr(vals)), NIL),
                                           current_env),
                                     callstack);
                o = pappl(0, car(vals));
                pappl_push(o, func);
                pappl_push(o, car(cdr(vals)));
                goto apply;
            }
        }
        else
        {
            if(!vals)           /* ('a) = 'a */
            {
                o = func;
                goto ret;
            }
            else                /* ('a f ...) = ((f a) ...) */
            {
                WITH_GC_PROTECTION()
                    callstack = cons(array(3,
                                           pappl(0, subr(subr_apply)),
                                           cons(cdr(vals), NIL),
                                           current_env),
                                     callstack);
                o = pappl(0, car(vals));
                pappl_push(o, func);
                goto apply;
            }
        }
    }
}

/* (eval O [ERRORBACK]) => evaluate O. on failure, call ERRORBACK with
 * error message, or error if ERRORBACK is omitted. */
DEFSUBR(subr_eval, E, E)(lobj args) { return eval(car(args), cdr(args) ? car(cdr(args)) : NIL); }

/* + OTHERS         ---------------- */

/* (quote ,O) => O. */
DEFSUBR(subr_quote, Q, _)(lobj args) { return car(args); }

/* (error MSG) => print MSG to error port and quit. */
DEFSUBR(subr_error, E, _)(lobj args) { lisp_error(string_ptr(car(args))); }

/* + INITIALIZATION ---------------- */

/* initialize current ports */
void subr_initialize()
{
    /* initialize ports */
    current_in = stdin, current_out = stdout, current_err = stderr;

    /* bind subrs */
    bind(intern("nil"), NIL);
    bind(intern("t"), intern("t"));
    bind(intern("nil?"), subr(subr_nilp));
    bind(intern("symbol?"), subr(subr_symbolp));
    bind(intern("gensym"), subr(subr_gensym));
    bind(intern("intern"), subr(subr_intern));
    bind(intern("bind!"), subr(subr_bind));
    bind(intern("set!"), subr(subr_set));
    bind(intern("bound-value"), subr(subr_bound_value));
    bind(intern("char?"), subr(subr_charp));
    bind(intern("char->int"), subr(subr_char_to_int));
    bind(intern("int->char"), subr(subr_int_to_char));
    bind(intern("integer?"), subr(subr_integerp));
    bind(intern("float?"), subr(subr_floatp));
    bind(intern("mod"), subr(subr_mod));
    bind(intern("/"), subr(subr_quot));
    bind(intern("round"), subr(subr_round));
    bind(intern("+"), subr(subr_add));
    bind(intern("*"), subr(subr_mult));
    bind(intern("-"), subr(subr_sub));
    bind(intern("div"), subr(subr_div));
    bind(intern("<="), subr(subr_le));
    bind(intern("<"), subr(subr_lt));
    bind(intern(">="), subr(subr_ge));
    bind(intern(">"), subr(subr_gt));
    bind(intern("stream?"), subr(subr_streamp));
    bind(intern("current-input-port"), subr(subr_input_port));
    bind(intern("current-output-port"), subr(subr_output_port));
    bind(intern("current-error-port"), subr(subr_error_port));
    bind(intern("set-ports"), subr(subr_set_ports));
    bind(intern("getc"), subr(subr_getc));
    bind(intern("putc"), subr(subr_putc));
    bind(intern("ungetc"), subr(subr_ungetc));
    bind(intern("open"), subr(subr_open));
    bind(intern("close"), subr(subr_close));
    bind(intern("cons?"), subr(subr_consp));
    bind(intern("cons"), subr(subr_cons));
    bind(intern("car"), subr(subr_car));
    bind(intern("cdr"), subr(subr_cdr));
    bind(intern("setcar!"), subr(subr_setcar));
    bind(intern("setcdr!"), subr(subr_setcdr));
    bind(intern("array?"), subr(subr_arrayp));
    bind(intern("make-array"), subr(subr_make_array));
    bind(intern("aref"), subr(subr_aref));
    bind(intern("aset!"), subr(subr_aset));
    bind(intern("string?"), subr(subr_stringp));
    bind(intern("function?"), subr(subr_functionp));
    bind(intern("fn"), subr(subr_fn));
    bind(intern("closure?"), subr(subr_closurep));
    bind(intern("closure"), subr(subr_closure));
    bind(intern("subr?"), subr(subr_subrp));
    bind(intern("dlsubr"), subr(subr_dlsubr));
    bind(intern("continuation?"), subr(subr_continuationp));
    bind(intern("eq?"), subr(subr_eq));
    bind(intern("char="), subr(subr_char_eq));
    bind(intern("="), subr(subr_num_eq));
    bind(intern("print"), subr(subr_print));
    bind(intern("read"), subr(subr_read));
    bind(intern("if"), subr(subr_if));
    bind(intern("evlis"), subr(subr_evlis));
    bind(intern("apply"), subr(subr_apply));
    bind(intern("unwind-protect"), subr(subr_unwind_protect));
    bind(intern("call-cc"), subr(subr_call_cc));
    bind(intern("eval"), subr(subr_eval));
    bind(intern("error"), subr(subr_error));
    bind(intern("quote"), subr(subr_quote));
}

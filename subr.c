#include "philisp.h"
#include "core.h"
#include "subr.h"

#include <dlfcn.h>              /* dlopen, dlsym */

#define unused(var) (void)(var) /* suppress "unused variable" warning */

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
    lobj value = cdr(args) ? car(cdr(args)) : NIL;
    bind(car(args), value, 0);
    return value;
}

/* (bound-value O [ERRORBACK]) => object which O is bound to. if O is
 * unbound, call ERRORBACK with error message, or error if ERRORBACK
 * is omitted. */
DEFSUBR(subr_bound_value, E, E)(lobj args)
{
    lobj pair = binding(car(args), 0);

    if(pair)
        return cdr(pair);

    else if(cdr(args))
    {
        lobj o;

        WITH_GC_PROTECTION()
            o = cons(car(cdr(args)),
                     cons(string("reference to unbound symbol."), NIL));

        return eval(o, NIL);    /* *FIXME* RECURSIVE "eval" */
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
        if(!integerp(car(lst)))
            return 0;
        else
            lst = cdr(lst);

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

    for(args = cdr(args), i = 1; args; args = cdr(args), i++)
    {
        if(!integerp(car(args)))
            type_error("subr \"/\"", i, "integer");
        val /= integer_value(car(args));
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

        for(; args; args = cdr(args))
            sum += integer_value(car(args));

        return integer(sum);
    }

    else
    {
        double sum = 0;
        unsigned ix;

        for(ix = 0; args; ix++, args = cdr(args))
            if(integerp(car(args)))
                sum += integer_value(car(args));
            else if(floatingp(car(args)))
                sum += floating_value(car(args));
            else
                type_error("subr \"+\"", ix, "number");

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

        for(; args; args = cdr(args))
            prod *= integer_value(car(args));

        return integer(prod);
    }

    else
    {
        double prod = 1.0;
        unsigned ix;

        for(ix = 0; args; ix++, args = cdr(args))
            if(integerp(car(args)))
                prod *= integer_value(car(args));
            else if(floatingp(car(args)))
                prod *= floating_value(car(args));
            else
                type_error("subr \"*\"", ix, "number");

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

            for(args = cdr(args); args; args = cdr(args))
                res -= integer_value(car(args));

            return integer(res);
        }

        else
        {
            double res;
            unsigned ix;

            if(integerp(car(args)))
                res = integer_value(car(args));
            else if(floatingp(car(args)))
                res = floating_value(car(args));
            else
                type_error("subr \"-\"", 0, "number");

            for(ix = 1, args = cdr(args); args; ix++, args = cdr(args))
                if(integerp(car(args)))
                    res -= integer_value(car(args));
                else if(floatingp(car(args)))
                    res -= floating_value(car(args));
                else
                    type_error("subr \"-\"", ix, "number");

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
        unsigned ix;

        if(integerp(car(args)))
            res = integer_value(car(args));
        else if(floatingp(car(args)))
            res = floating_value(car(args));
        else
            type_error("subr \"/\"", 0, "number");

        for(ix = 1, args = cdr(args); args; ix++, args = cdr(args))
            if(integerp(car(args)))
                res /= integer_value(car(args));
            else if(floatingp(car(args)))
                res /= floating_value(car(args));
            else
                type_error("subr \"/\"", ix, "number");

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
            unsigned ix;                                                \
            lobj last;                                                  \
                                                                        \
            if(integerp(car(args)))                                     \
                num1 = integer_value(car(args));                        \
            else if(floatingp(car(args)))                               \
                num1 = floating_value(car(args));                       \
            else                                                        \
                type_error("subr \"" #name "\"", 0, "number");          \
                                                                        \
            for(ix = 1, args = cdr(args); args; ix++, args = cdr(args)) \
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
                num1 = num2, last = args;                               \
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

    else if(args)
    {
        lobj o;

        WITH_GC_PROTECTION()
            o = cons(car(args),
                     cons(string("failed to get character."), NIL));

        return eval(o, NIL);    /* *FIXME* RECURSIVE "eval" */
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
        if(cdr(args))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)),
                         cons(string("failed to put character"), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
        }

        else
            lisp_error("failed to put character.");
    }

    fflush(current_out);

    return car(args);
}

/* (puts STRING [ERRORBACK]) => write STRING to output port and return
 * STRING. on failure, ERRORBACK is called with error message, or
 * error if ERRORBACK is omitted. */
DEFSUBR(subr_puts, E, E)(lobj args)
{
    if(!stringp(car(args)))
        type_error("subr \"puts\"", 0, "string");

    if(fprintf(current_out, string_ptr(car(args))) < 0)
    {
        if(cdr(args))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)),
                         cons(string("failed to put string"), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
        }

        else
            lisp_error("failed to put string.");
    }

    fflush(current_out);

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
        if(cdr(args))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)),
                         cons(string("failed to unget character."), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
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
        if(cdr(args))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)), cons(string("failed to open file"), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
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
        if(cdr(args))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(args)), cons(string("failed to close stream."), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
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
DEFSUBR(subr_aset, E E E, _)(lobj args)
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
                tail = cdr(tail), pattern = pattern | (mask & ~0), len++;
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
    lobj o = car(args);

    if(!(functionp(o) || symbolp(o) || consp(o)))
        type_error("subr \"closure\"", 0, "function, symbol, nor cons");

    WITH_GC_PROTECTION()
        o = closure(car(args), save_current_env(0));

    return o;
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
        if(cdr(cdr(args)))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(cdr(args))),
                         cons(string("filed to load shared object."), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
        }

        else
            lisp_error("failed to load shared object.");
    }

    if(!stringp(car(cdr(args))))
        type_error("subr \"dlsubr\"", 1, "string");

    if(!(ptr = dlsym(h, string_ptr(car(cdr(args))))))
    {
        if(cdr(cdr(args)))
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(cdr(cdr(args))),
                         cons(string("filed to find symbol from shared object."), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
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

        for(args = cdr(args); args; args = cdr(args))
        {
            if(last != car(args)) return NIL;
            last = car(args);
        }

        return symbol();
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
        unsigned ix;
        lobj last;

        if(characterp(car(args)))
            ch1 = character_value(car(args));
        else
            type_error("subr \"char=\"", 0, "character");

        for(ix = 1, args = cdr(args); args; ix++, args = cdr(args))
        {
            if(characterp(car(args)))
                ch2 = character_value(car(args));
            else
                type_error("subr \"char=\"", ix, "character");

            if(ch1 != ch2) return NIL;

            ch1 = ch2, last = args;
        }

        return car(last);
    }
}

/* (= NUM1 ...) => last number if NUM1 ... are all equal as numbers,
 * or () otherwise. if no numbers are given, return an unspecified
 * non-() value. */
DEFINE_ORD_SUBR(subr_num_eq, ==)

/* + UNPARSER       ---------------- */

/* (print O) => print string representation of object O to output port
 * and return O. */
DEFSUBR(subr_print, E, _)(lobj args)
{
    print(current_out, car(args));
    return car(args);
}

/* + PARSER         ---------------- */

/* (read [ERRORBACK]) => read an S-expression from input port. on
 * failure, ERRORBACK is called with error message, or error if
 * ERRORBACK is omitted. */
DEFSUBR(subr_read, _, E)(lobj args)
{
    lobj val;
    unused(args);

    val = read();

    if(last_parse_error)
    {
        if(args)
        {
            lobj o;

            WITH_GC_PROTECTION()
                o = cons(car(args), cons(string(last_parse_error), NIL));

            return eval(o, NIL); /* *FIXME* RECURSIVE "eval" */
        }

        else
            lisp_error(last_parse_error);
    }

    return val;
}

/* + OTHERS         ---------------- */

/* (quote ,O) => O. */
DEFSUBR(subr_quote, Q, _)(lobj args) { return car(args); }

/* (error MSG) => print MSG to error port and quit. */
DEFSUBR(subr_error, E, _)(lobj args) { lisp_error(string_ptr(car(args))); }

/* + INITIALIZE     ---------------- */

void subr_initialize()
{
    bind(intern("nil"), NIL, 0);
    bind(intern("nil?"), subr(subr_nilp), 0);
    bind(intern("symbol?"), subr(subr_symbolp), 0);
    bind(intern("gensym"), subr(subr_gensym), 0);
    bind(intern("intern"), subr(subr_intern), 0);
    bind(intern("bind!"), subr(subr_bind), 0);
    bind(intern("bound-value"), subr(subr_bound_value), 0);
    bind(intern("char?"), subr(subr_charp), 0);
    bind(intern("char->int"), subr(subr_char_to_int), 0);
    bind(intern("int->char"), subr(subr_int_to_char), 0);
    bind(intern("integer?"), subr(subr_integerp), 0);
    bind(intern("float?"), subr(subr_floatp), 0);
    bind(intern("mod"), subr(subr_mod), 0);
    bind(intern("/"), subr(subr_quot), 0);
    bind(intern("round"), subr(subr_round), 0);
    bind(intern("+"), subr(subr_add), 0);
    bind(intern("*"), subr(subr_mult), 0);
    bind(intern("-"), subr(subr_sub), 0);
    bind(intern("div"), subr(subr_div), 0);
    bind(intern("<="), subr(subr_le), 0);
    bind(intern("<"), subr(subr_lt), 0);
    bind(intern(">="), subr(subr_ge), 0);
    bind(intern(">"), subr(subr_gt), 0);
    bind(intern("stream?"), subr(subr_streamp), 0);
    bind(intern("current-input-port"), subr(subr_input_port), 0);
    bind(intern("current-output-port"), subr(subr_output_port), 0);
    bind(intern("current-error-port"), subr(subr_error_port), 0);
    bind(intern("set-ports"), subr(subr_set_ports), 0);
    bind(intern("getc"), subr(subr_getc), 0);
    bind(intern("putc"), subr(subr_putc), 0);
    bind(intern("puts"), subr(subr_puts), 0);
    bind(intern("ungetc"), subr(subr_ungetc), 0);
    bind(intern("open"), subr(subr_open), 0);
    bind(intern("close"), subr(subr_close), 0);
    bind(intern("cons?"), subr(subr_consp), 0);
    bind(intern("cons"), subr(subr_cons), 0);
    bind(intern("car"), subr(subr_car), 0);
    bind(intern("cdr"), subr(subr_cdr), 0);
    bind(intern("setcar!"), subr(subr_setcar), 0);
    bind(intern("setcdr!"), subr(subr_setcdr), 0);
    bind(intern("array?"), subr(subr_arrayp), 0);
    bind(intern("make-array"), subr(subr_make_array), 0);
    bind(intern("aref"), subr(subr_aref), 0);
    bind(intern("aset!"), subr(subr_aset), 0);
    bind(intern("string?"), subr(subr_stringp), 0);
    bind(intern("function?"), subr(subr_functionp), 0);
    bind(intern("fn"), subr(subr_fn), 0);
    bind(intern("closure?"), subr(subr_closurep), 0);
    bind(intern("closure"), subr(subr_closure), 0);
    bind(intern("subr?"), subr(subr_subrp), 0);
    bind(intern("dlsubr"), subr(subr_dlsubr), 0);
    bind(intern("continuation?"), subr(subr_continuationp), 0);
    bind(intern("eq?"), subr(subr_eq), 0);
    bind(intern("char="), subr(subr_char_eq), 0);
    bind(intern("="), subr(subr_num_eq), 0);
    bind(intern("print"), subr(subr_print), 0);
    bind(intern("read"), subr(subr_read), 0);
    bind(intern("error"), subr(subr_error), 0);
    bind(intern("quote"), subr(subr_quote), 0);
}

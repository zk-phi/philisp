#include "structures.h"
#include "core.h"

#include <stdlib.h>             /* exit */
#include <ctype.h>              /* isspace */
#include <string.h>             /* strchr */

#define unused(var) (void)(var) /* suppress "unused variable" warning */

/* + ENVIRONMENT    ---------------- */

/* local_env  = '(<push here> (x . 1) ... NIL ... (y . 2) ... NIL ... (z . 3))
 * global_env = '(NIL <push here> (nil . ()) (t . t) (cons . #<subr cons>) ...)
 */
lobj local_env, global_env, callstack, unwind_protects;
FILE *current_in, *current_out, *current_err;

/* push boundary to current environ */
void env_boundary() { local_env = cons(NIL, local_env); }

/* search for a binding of O. returns binding, or () if unbound. if
 * LOCAL is non-0, search only before a boundary. */
lobj binding(lobj o, int local)
{
    lobj env;

    for(env = local_env; env; env = cdr(env))
        if(!car(env) && local)
            return NIL;
        else if(car(env) && car(car(env)) == o)
            return car(env);

    if(!local)
        for(env = cdr(global_env); env; env = cdr(env))
            if(car(car(env)) == o)
                return car(env);

    return NIL;
}

/* if binding of O is found, modify the binding. otherwise add a new
 * global binding of O. if LOCAL is non-0, search binding only BEFORE
 * A BOUNDARY, and add a local binding instead. */
void bind(lobj o, lobj value, int local)
{
    lobj b;

    if((b = binding(o, local)))
        setcdr(b, value);
    else if(local)
        WITH_GC_PROTECTION()
            local_env = cons(cons(o, value), local_env);
    else
        WITH_GC_PROTECTION()
            setcdr(global_env, cons(cons(o, value), cdr(global_env)));
}

lobj save_current_env(int local)
{
    lobj o;

    local_env = cons(NIL, local_env);

    if(local)
        return cons(local_env, global_env);
    else
    {
        WITH_GC_PROTECTION()
            o = cons(local_env, cons(NIL, cdr(global_env)));
        return o;
    }
}

void restore_current_env(lobj env) { local_env = cdr(car(env)), global_env = cdr(env); }

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

void stack_dump(FILE* stream)
{
    lobj stack, t;
    unsigned level = 0, i;

    for(stack = callstack; stack; stack = cdr(stack))
    {
        for(i = 0; i < level; i++) fprintf(stream, "  ");
        fprintf(stream, "> in expression ");
        level++;

        putc('(', stream);

        if(pap(t = array_ptr(car(stack))[0]))
        {
            print(stream, pa_function(t)), putc(' ', stream);

            for(t = pa_values(t); t; t = cdr(t))
                print(stream, car(t)), putc(' ', stream);
        }

        fprintf(stream, "[!] ");

        for(t = array_ptr(car(stack))[1]; t; t = cdr(t))
            putc(' ', stream), print(stream, car(t));

        fprintf(stream, ")\n");
    }
}

/* *TODO* IMPLEMENT ERROR HANDLER */

void type_error(char* name, unsigned ix, char* expected)
{
    fprintf(current_err,
            "TYPE ERROR: %d-th arg for %s is not a %s\n",
            ix, name, expected);
    stack_dump(current_err);
    exit(1);
}

void lisp_error(char* msg)
{
    fprintf(current_err, "ERROR: %s\n", msg);
    stack_dump(current_err);
    exit(1);
}

void fatal(char* msg)
{
    fprintf(current_err, "FATAL: %s\n", msg);
    stack_dump(current_err);
    exit(1);
}

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
            fprintf(stream, "\\x%02x", (int)ch & 0xFF);
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

        if(consp(expr))
        {
            fprintf(stream, "(");

            if(consp(car(expr)))
                fprintf(stream, "(...)");
            else if(arrayp(car(expr)))
                fprintf(stream, "[...]");
            else
                print(stream, car(expr));

            fprintf(stream, " ...)");
        }
        else if(arrayp(expr))
            fprintf(stream, "[...]");
        else
            print(stream, expr);

        putc('>', stream);
    }

    else if(closurep(o))
    {
        pargs args = function_args(closure_obj(o));
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

    else if(pap(o))
    {
        fprintf(stream, "#<func (pa:");
        print(stream, pa_function(o));
        fprintf(stream, "/%d)>", pa_num_values(o));
    }

    else
        fprintf(stream, "#<broken object?>");

    fflush(stream);
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
                        v = v * 16 + (ch - '0');
                    else if('a' <= ch && ch <= 'f')
                        v = v * 16 + (ch - 'a') + 10;
                    else if('A' <= ch && ch <= 'F')
                        v = v * 16 + (ch - 'A') + 10;
                    else
                        ungetc(ch, current_in);
                }

                return v;
            }
        }

        return -3;
    }
}

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

      case ';':                 /* comment */
        ch = getc(current_in);
        while(ch != '\n' && ch != EOF)
            ch = getc(current_in);
        return read();

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

                /* *FIXME* MAY OVERFLOW */
                ch = getc(current_in);
                while('0' <= ch && ch <= '9')
                {
                    vv = vv * 10 + (ch - '0');
                    ch = getc(current_in);
                }
                while(vv >= 1) vv /= 10;

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
                fatal("unexpected non-number value after '-'.");
        }

        /* vv FALL THROUGH ... vv */

      default:                  /* symbol name */
        while(1)
        {
            if(bufptr == SYMBOL_NAME_MAX)
                fatal("too long symbol name given.");

            else if(ch == -1 || isspace(ch) || strchr("()[]\";", ch))
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

/* + EVALUATOR      ---------------- */

#define DEFINE_DUMMY_SUBR(n, a, r)                     \
    DEFSUBR(n, a, r)(lobj args)                        \
    {                                                  \
        unused(args);                                  \
        fatal("unexpected call to " #n ".");  \
    }                                                  \

/* (if COND ,THEN [,ELSE]) => if COND is non-(), evaluate THEN, else
 * evaluate ELSE. if ELSE is omitted, return (). */
DEFINE_DUMMY_SUBR(subr_if, E Q Q, _)

/* (evlis PROC EXPRS) => evaluate list of expressions in accordance
 * with evaluation rule of PROC. */
DEFINE_DUMMY_SUBR(subr_evlis, E E, _)

/* (apply PROC ARGS) => apply ARGS to PROC. */
DEFINE_DUMMY_SUBR(subr_apply, E E, _)

/* (unwind-protect ,BODY ,AFTER) => evaluate BODY and then AFTER. when
 * a continuation is called in BODY, evaluate AFTER before winding the
 * continuation. */
DEFINE_DUMMY_SUBR(subr_unwind_protect, Q Q, _)

/* (call/cc FUNC) => evaluate BODY and then AFTER. when
 * a continuation is called in BODY, evaluate AFTER before winding the
 * continuation. */
DEFINE_DUMMY_SUBR(subr_call_cc, E, _)

/* (eval O [ERRORBACK]) => evaluate O. on failure, call ERRORBACK with
 * error message, or error if ERRORBACK is omitted. */
DEFINE_DUMMY_SUBR(subr_eval, E, E)

int eval_pattern(lobj o)
{
    if(functionp(o))
        return function_args(o) >> 9;
    else if(closurep(o))
        return function_args(closure_obj(o)) >> 9;
    else if(subrp(o))
        return subr_args(o) >> 9;
    else if(continuationp(o))
        return 1;
    else if(pap(o))
        return pa_eval_pattern(o);
    else
        return ~0;
}

/* *FIXME* RECURSIVE "eval" */
#define EVALUATION_ERROR(str)                                   \
    do{                                                         \
        if(!errorback)                                          \
            lisp_error(str);                                    \
        else                                                    \
        {                                                       \
            lobj o;                                             \
            WITH_GC_PROTECTION()                                \
                o = cons(errorback, cons(string(str), NIL));    \
            return eval(o, NIL);                                \
        }                                                       \
    }                                                           \
    while(0)

#if DEBUG
#define DEBUG_DUMP(labelname)                               \
    do{                                                     \
        lobj env, stack;                                    \
        for(stack = callstack; stack; stack = cdr(stack))   \
            printf("> ");                                   \
        printf(labelname ": "); print(stdout, o);           \
        printf(" | locals: ");                              \
        for(env = local_env; env; env = cdr(env))           \
            if(car(env))                                    \
            {                                               \
                putchar('(');                               \
                print(stdout, car(car(env)));               \
                printf(" : ");                              \
                print(stdout, cdr(car(env)));               \
                printf(") ");                               \
            }                                               \
            else                                            \
                printf("/ ");                               \
        printf(" | globals: ");                             \
        for(env = cdr(global_env); env; env = cdr(env))     \
            if(subrp(cdr(car(env))))                        \
            {                                               \
                printf("...");                              \
                break;                                      \
            }                                               \
            else                                            \
            {                                               \
                                                            \
                putchar('(');                               \
                print(stdout, car(car(env)));               \
                printf(" : ");                              \
                print(stdout, cdr(car(env)));               \
                printf(") ");                               \
            }                                               \
        printf("\n"); fflush(stdout);                       \
    }while(0)
      #endif
      #if !DEBUG
      #define DEBUG_DUMP(labelname) do{}while(0)
        #endif

/* *FIXME* "eval" INSIDE "eval" BREAKS CALLSTACK */
lobj eval(lobj o, lobj errorback)
{
    /* /\* we already have an eval session */
    /*    -> just push to stack and return a dummy obj *\/ */
    /* if(callstack) */
    /* { */
    /*     WITH_GC_PROTECTION() */
    /*     { */
    /*         lobj app = pa(0, subr(subr_eval)); */
    /*         pa_push(app, o); */
    /*         callstack = cons(array(4, app, NIL, local_env, global_env), callstack); */
    /*     } */
    /*     return NIL; */
    /* } */

    callstack = NIL;

  eval:                 /* here O is an expression to be evaluated. */

    DEBUG_DUMP("eval");

    if(symbolp(o))
    {
        if(!(o = binding(o, 0)))
            EVALUATION_ERROR("reference to unbound symbol.");
        o = cdr(o);
        goto ret;
    }
    else if(consp(o))
    {
        WITH_GC_PROTECTION() /* stack_frame = [pa, pending_args, local, global] */
            callstack = cons(array(3, NIL, cdr(o), save_current_env(1)), callstack);
        o = car(o);
        goto eval;
    }

  ret:                       /* here O is an object evaluated just now. */

    DEBUG_DUMP("ret ");

    if(!callstack)           /* nothing more to evaluate */
        return o;
    else
    {
        lobj *ptr = array_ptr(car(callstack));

        /* update pa */
        if(!ptr[0])             /* pa is not set */
            ptr[0] = pa(eval_pattern(o), o);
        else
            pa_push(ptr[0], o);

        /* restore environ */
        restore_current_env(ptr[2]);

        /* then evaluate next "unevaluated" arg or apply all evaluated args */
        if(ptr[1])
        {
            o = car(ptr[1]);
            ptr[1] = cdr(ptr[1]);

            if(pa_eval_pattern(ptr[0]) & 1)
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

  apply:                  /* here O is a pa object to be applied */

    DEBUG_DUMP("call");

    {
        lobj func = pa_function(o),
             vals = pa_values(o);
        int num_vals = pa_num_values(o);

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
                        bind(car(formals), car(vals), 1);
                        vals = cdr(vals), formals = cdr(formals);
                    }
                    else
                    {
                        bind(formals, vals, 1);
                        break;
                    }
                }

                o = function_expr(func);
                goto eval;
            }
        }
        else if(closurep(func))
        {
            pargs num_args = function_args(closure_obj(func));

            if((num_args & 255) < num_vals && !(num_args & 256)) /* too many */
                EVALUATION_ERROR("too many arguments applied to a closure.");
            else if(num_vals < (num_args & 255)) /* too few */
                goto ret;
            else                /* okay */
            {
                restore_current_env(closure_env(func));
                pa_set_function(o, closure_obj(func));
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

                if(fobj == f_subr_eval)
                {
                    o = car(vals);
                    /* *FIXME* FIX ERROR HANDLER */
                    /* errorback = cdr(vals) ? car(cdr(vals)) : errorback; */
                    goto eval;
                }
                if(fobj == f_subr_if)
                {
                    o = car(vals) ? car(cdr(vals)) : car(cdr(cdr(vals)));
                    goto eval;
                }
                else if(fobj == f_subr_evlis)
                {
                    /* *TODO* IMPLEMENT EVLIS */
                    fatal("NOT IMPLEMENTED subr \"evlis\".");
                }
                else if(fobj == f_subr_apply)
                {
                    o = pa(eval_pattern(car(vals)), car(vals));

                    if(!listp(car(cdr(vals))))
                        type_error("subr \"apply\"", 1, "list");

                    for(vals = car(cdr(vals)); vals; vals = cdr(vals))
                        pa_push(o, car(vals));

                    goto apply;
                }
                else if(fobj == f_subr_unwind_protect)
                {
                    fatal("NOT IMPLEMENTED subr \"unwind-protect.\"");

                    /* *TODO* IMPLEMENT SUBR "unwind-protect" */

                    /* unwind_protects = cons(car(cdr(vals)), unwind_protects); */
                    /* o = car(vals); */
                    /* goto eval; */
                }
                else if(fobj == f_subr_call_cc)
                {
                    o = pa(eval_pattern(car(vals)), car(vals));
                    pa_push(o, continuation(callstack));
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
        else if(pap(func))
        {
            /* *FIXME* EFFICIENCY */

            lobj vals2 = pa_values(func);

            o = pa(eval_pattern(pa_function(func)), pa_function(func));

            while(vals2)
            {
                pa_push(o, car(vals2));
                vals2 = cdr(vals2);
            }

            while(vals)
            {
                pa_push(o, car(vals));
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
                o = pa(eval_pattern(car(vals)), func);
                pa_push(o, car(vals));
                goto ret;
            }
            else                /* (1 f 2 ...) = ((f 1 2) ...) */
            {
                WITH_GC_PROTECTION()
                    callstack = cons(array(4,
                                           pa(0, subr(subr_apply)),
                                           cons(cdr(cdr(vals)), NIL),
                                           save_current_env(1)),
                                     callstack);
                o = pa(0, car(vals));
                pa_push(o, func);
                pa_push(o, car(cdr(vals)));
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
                    callstack = cons(array(4,
                                           pa(0, subr(subr_apply)),
                                           cons(cdr(vals), NIL),
                                           save_current_env(1)),
                                     callstack);
                o = pa(0, car(vals));
                pa_push(o, func);
                goto apply;
            }
        }
    }
}

/* + INITIALIZE     ---------------- */

/* initialize current ports */
void core_initialize()
{
    current_in = stdin, current_out = stdout, current_err = stderr;
    local_env = callstack = unwind_protects = NIL, global_env = cons(NIL, NIL);

    bind(intern("if"), subr(subr_if), 0);
    bind(intern("evlis"), subr(subr_evlis), 0);
    bind(intern("apply"), subr(subr_apply), 0);
    bind(intern("unwind-protect"), subr(subr_unwind_protect), 0);
    bind(intern("call-cc"), subr(subr_call_cc), 0);
    bind(intern("eval"), subr(subr_eval), 0);
}

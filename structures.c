#include "structures.h"

#include <stdio.h>              /* puts, putc, getc */
#include <stdlib.h>             /* exit, malloc, free */
#include <string.h>             /* strlen */
#include <stdarg.h>             /* va_start, va_list, va_end */

/* + TYPE_TAGS      ---------------- */

#define TYPE_SYMB  0  /* symbol                                            */
#define TYPE_CHAR  1  /* char                                              */
#define TYPE_INT   2  /* int                                               */
#define TYPE_FLOAT 3  /* float                                             */
#define TYPE_STRM  4  /* stream (= FILE*)                                  */
#define TYPE_CONS  5  /* cons         : lobj + lobj                        */
#define TYPE_ARR   6  /* array        : length + (lobj, lobj, lobj, ...)   */
#define TYPE_STR   7  /* (STRs are distinguished from ARRs INTERNALLY)     */
#define TYPE_SUBR  8  /* C-function   : arity + pointer to (lobj(*)(lobj)) */
#define TYPE_FUNC  9  /* function     : formals + body                     */
#define TYPE_CLOS  10 /* closure      : function or subr + bindings        */
#define TYPE_CONT  11 /* continuation : call stack + environ               */
#define TYPE_PA    12 /* (internal structure for partially-applied object) */

/* + ALLOCATOR      ---------------- */

/* *TODO* IMPLEMENT BETTER ALLOCATOR */
/* *TODO* IMPLEMENT GC */

unsigned int gc_protected = 0, gc_protect_count = 0;
lobj gc_protected_items[GC_PROTECT_MAX];

void gc_protect(lobj o)
{
    /* we cannot alloc any lisp objects here (so use array) */
    if(gc_protect_count == GC_PROTECT_MAX)
    {
        fputs("INTERNAL ERROR: GC protection error.", stderr);
        exit(1);
    }
    else
        gc_protected_items[gc_protect_count++] = o;
}

lobj alloc_lobj(int type, size_t data_size)
{
    lobj o = (lobj)malloc(sizeof(struct lobj) + data_size - 1);
    o->mark = 0, o->type = type;
    if(gc_protected) gc_protect(o);
    return o;
}

void (*free_lobj)(lobj) = (void(*)(lobj))free;

/* + SYMBOL         ---------------- */

/* *TODO* REDUCE MEMORY CONSUMPTION */
/* *TODO* IMPROVE REVERSE-INTERN EFFICIENCY */

int symbolp(lobj o) { return o && o->type == TYPE_SYMB; }
lobj symbol() { return alloc_lobj(TYPE_SYMB, 0); }

/* -- intern -- */

/* symbol tree is a trie tree of (string -> symbol) */
typedef struct table_node *table_node;
struct table_node { table_node bros, child; char ch; lobj symb; };

table_node symbol_table = NULL;

/* search for a symbol associated with NAME. if it does not exist,
 * make it. */
lobj intern(char* name)
{
    table_node *tptr = &symbol_table;

    while(1)
    {
        if(!*tptr)
        {
            *tptr = (table_node)malloc(sizeof(struct table_node));
            (*tptr)->bros = (*tptr)->child = NULL,
            (*tptr)->symb = NIL, (*tptr)->ch = *name;

            if(*name == '\0')
                return (*tptr)->symb = symbol();

            tptr = &((*tptr)->child), name++;
        }

        else if(*name == (*tptr)->ch)
        {
            if(*name == '\0')
                return (*tptr)->symb ? (*tptr)->symb : ((*tptr)->symb = symbol());

            tptr = &((*tptr)->child), name++;
        }

        else
            tptr = &((*tptr)->bros);
    }
}

/* -- reverse intern -- */

int rintern_(lobj symbol, char* name, table_node table, unsigned size)
{
    if(!size)
        return 1;

    while(table)
    {
        *name = table->ch;

        if(table->symb == symbol)
            return 0;

        else if(!rintern_(symbol, name + 1, table->child, size - 1))
            return 0;

        else
            table = table->bros;
    }

    return 1;
}

/* dump name which SYMBOL is associated with, to buffer NAME. iff
 * SYMBOL is not associated with any name, or name is longer than
 * SIZE, return non-0 value. */
int rintern(lobj symbol, char* name, unsigned size)
{
    return rintern_(symbol, name, symbol_table, size);
}

/* + CHAR           ---------------- */

int characterp(lobj o) { return o && o->type == TYPE_CHAR; }
char character_value(lobj o) { return *(o->data); }

lobj character(char ch)
{
    lobj o = alloc_lobj(TYPE_CHAR, 1);
    *(char*)(o->data) = ch;
    return o;
}

/* + INT            ---------------- */

int integerp(lobj o) { return o && o->type == TYPE_INT; }
int integer_value(lobj o) { return *(int*)(o->data); }

lobj integer(int i)
{
    lobj o = alloc_lobj(TYPE_INT, sizeof(int));
    *(int*)(o->data) = i;
    return o;
}

/* + FLOAT          ---------------- */

int floatingp(lobj o) { return o && o->type == TYPE_FLOAT; }
double floating_value(lobj o) { return *(double*)(o->data); }

lobj floating(double d)
{
    lobj o = alloc_lobj(TYPE_FLOAT, sizeof(double));
    *(double*)(o->data) = d;
    return o;
}

/* + STREAM         ---------------- */

int streamp(lobj o) { return o && o->type == TYPE_STRM; }
FILE* stream_value(lobj o) { return *(FILE**)(o->data); }

lobj stream(FILE *f)
{
    lobj o = alloc_lobj(TYPE_STRM, sizeof(FILE*));
    *(FILE**)(o->data) = f;
    return o;
}

/* + CONS           ---------------- */

int consp(lobj o) { return o && o->type == TYPE_CONS; }
lobj car(lobj o) { return ((lobj*)(o->data))[0]; }
lobj cdr(lobj o) { return ((lobj*)(o->data))[1]; }

lobj cons(lobj car, lobj cdr)
{
    lobj o = alloc_lobj(TYPE_CONS, sizeof(lobj) * 2);
    ((lobj*)(o->data))[0] = car;
    ((lobj*)(o->data))[1] = cdr;
    return o;
}

void setcar(lobj o, lobj newcar) { ((lobj*)(o->data))[0] = newcar; }
void setcdr(lobj o, lobj newcdr) { ((lobj*)(o->data))[1] = newcdr; }

/* (proper) list */

int listp(lobj o) { return consp(o) ? listp(cdr(o)) : !o; }

lobj list(unsigned len, ...)
{
    if(!len)
        return NIL;
    else
    {
        va_list rest;
        lobj o, last;

        va_start(rest, len);

        o = cons(va_arg(rest, lobj), NIL), len--;

        WITH_GC_PROTECTION()
            for(last = o; len--; last = cdr(last))
                setcdr(last, cons(va_arg(rest, lobj), NIL));

        va_end(rest);

        return o;
    }
}

/* + ARRAY          ---------------- */

int arrayp(lobj o) { return o && o->type == TYPE_ARR; }
unsigned array_length(lobj o) { return ((unsigned*)(o->data))[0]; }
lobj* array_ptr(lobj o) { return (lobj*)&(((unsigned*)(o->data))[1]); }

lobj alloc_array(unsigned size)
{
    lobj o = alloc_lobj(TYPE_ARR, sizeof(unsigned) + sizeof(lobj) * size);
    ((unsigned*)(o->data))[0] = size;
    return o;
}

lobj make_array(unsigned size, lobj init)
{
    lobj o = alloc_array(size), *ptr;

    for(ptr = array_ptr(o); size--; ptr++)
        *ptr = init;

    return o;
}

lobj array(unsigned size, ...)
{
    va_list values;
    lobj o = alloc_array(size), *ptr;

    for(ptr = array_ptr(o), va_start(values, size); size--; ptr++)
        *ptr = va_arg(values, lobj);

    va_end(values);

    return o;
}

/* + STRING         ---------------- */

unsigned (*string_length)(lobj o) = array_length;
char* (*string_ptr)(lobj o) = (char*(*)(lobj))array_ptr;

lobj make_string(unsigned len, char init)
{
    lobj o = alloc_array(len);
    char *ptr;

    o->type = TYPE_STR;

    for(ptr = string_ptr(o); len--; ptr++)
        *ptr = init;
    *ptr = '\0';

    return o;
}

lobj string(char* str)
{
    unsigned len = strlen(str);
    lobj o = alloc_array(len);
    char *ptr;

    o->type = TYPE_STR;

    for(ptr = string_ptr(o); len--; ptr++, str++)
        *ptr = *str;
    *ptr = '\0';

    return o;
}

/* destructively transform into an array */
void string_to_array(lobj o)
{
    char *str = string_ptr(o);
    lobj *arr = (lobj*)str;
    unsigned len = string_length(o), ix;

    for(ix = len - 1; ix < len; ix--)
        arr[ix] = character(str[ix]);

    o->type = TYPE_ARR;
}

int stringp(lobj o)
{
    if(!o)
        return 0;

    else if(o->type == TYPE_STR)
        return 1;

    /* transform into a string if possible */
    else if(o->type == TYPE_ARR)
    {
        unsigned len = array_length(o), ix;
        lobj *arr = array_ptr(o);
        char *dest = (char*)array_ptr(o),
              *src = (char*)malloc(len); /* *NOTE* "malloc" USED HERE */

        for(ix = 0; ix < len; ix++)
            if(!characterp(arr[ix]))
                return 0;

        for(ix = 0; ix < len; ix++)
            src[ix] = character_value(arr[ix]);

        for(ix = 0; ix < len; ix++)
            dest[ix] = src[ix];
        dest[ix] = '\0';

        o->type = TYPE_STR;

        return 1;
    }

    return 0;
}

/* + FUNCTION       ---------------- */

int functionp(lobj o) { return o && o->type == TYPE_FUNC; }
pargs function_args(lobj o) { return ((pargs*)(o->data))[0]; }
lobj function_formals(lobj o) { return ((lobj*)&(((pargs*)(o->data))[1]))[0]; }
lobj function_expr(lobj o) { return ((lobj*)&(((pargs*)(o->data))[1]))[1]; }

lobj function(pargs args, lobj formals, lobj expr)
{
    lobj o = alloc_lobj(TYPE_FUNC, sizeof(int) + 2 * sizeof(lobj));
    ((pargs*)(o->data))[0] = args;
    ((lobj*)&(((pargs*)(o->data))[1]))[0] = formals;
    ((lobj*)&(((pargs*)(o->data))[1]))[1] = expr;
    return o;
}

/* + CLOSURE        ---------------- */

int closurep(lobj o) { return o && o->type == TYPE_CLOS; }
lobj (*closure_obj)(lobj) = car;
lobj (*closure_env)(lobj) = cdr;

lobj closure(lobj obj, lobj env)
{
    lobj o = cons(obj, env);
    o->type = TYPE_CLOS;
    return o;
}

/* + C-FUNCTION     ---------------- */

/* a subr is a lisp function implemented in C */

int subrp(lobj o) { return o && o->type == TYPE_SUBR; }
lsubr subr_object(lobj o) { return (*(lsubr*)(o->data)); }
pargs subr_args(lobj o) { return (*(lsubr*)(o->data)).args; }
lobj (*subr_function(lobj o))(lobj) { return (*(lsubr*)(o->data)).function; }
char* subr_description(lobj o) { return (*(lsubr*)(o->data)).description; }

lobj subr(lsubr subr)
{
    lobj o = alloc_lobj(TYPE_SUBR, 1 + sizeof(lsubr));
    *(lsubr*)(o->data) = subr;
    return o;
}

/* + CONTINUATION   ---------------- */

int continuationp(lobj o) { return o && o->type == TYPE_CONT; }
lobj continuation_callstack(lobj o) { return *(lobj*)(o->data); }

lobj continuation(lobj callstack)
{
    lobj o = alloc_lobj(TYPE_CONT, sizeof(lobj));
    o->type = TYPE_CONT;
    *(lobj*)(o->data) = callstack;
    return o;
}

/* + PA          ---------------- */

/* partially-applied object
   example: (if 1 'a)
   -> args = 0000 .... 0000 0000 0001
   -> head = cons(#<subr if> (cons 1 @tail=cons('a, NIL)))
 */

int pap(lobj o) { return o && o->type == TYPE_PA; }
int pa_eval_pattern(lobj o) { return ((int*)(o->data))[0]; }
int pa_num_values(lobj o) { return ((int*)(o->data))[1]; }
lobj pa_function(lobj o) { return car(((lobj*)&(((int*)(o->data))[2]))[0]); }
void pa_set_function(lobj o, lobj newfn) { setcar(((lobj*)&(((int*)(o->data))[2]))[0], newfn); }
lobj pa_values(lobj o) { return cdr(((lobj*)&(((int*)(o->data))[2]))[0]); }

lobj pa(int eval_pattern, lobj function)
{
    lobj o = alloc_lobj(TYPE_PA, sizeof(int) * 2 + sizeof(lobj) * 2);
    ((int*)(o->data))[0] = eval_pattern;
    ((int*)(o->data))[1] = 0;
    ((lobj*)&(((int*)(o->data))[2]))[0]
        = ((lobj*)&(((int*)(o->data))[2]))[1] = cons(function, NIL);
    return o;
}

/* *NOTE* NOT PORTABLE IMPLEMENTATION OF "eval_pattern"
   - ">>" FOR AN "int" SHOULD NOT BE LOGICAL
   - NUMBER OF MAXIMUM ARGUMENTS DEPENDS ON WORD-SIZE
*/

void pa_push(lobj o, lobj v)
{
    lobj t = cons(v, NIL);
    ((int*)(o->data))[0] >>=  1;
    ((int*)(o->data))[1]++;
    setcdr(((lobj*)&(((int*)(o->data))[2]))[1], t);
    ((lobj*)&(((int*)(o->data))[2]))[1] = t;
}

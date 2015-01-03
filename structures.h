#ifndef _PHI_H_
#define _PHI_H_ /* _PHI_H_ */
/* ---------------- ---------------- ---------------- ---------------- */

#include <stdio.h>              /* FILE* */

/* --- configs --- */

#define DEBUG           1    /* enable debug output */
#define SYMBOL_NAME_MAX 50   /* maximum length of symbol name */
#define GC_PROTECT_MAX  100  /* maximum number of protected objects */

/* --- typedefs --- */

/* lobj: lisp object */

typedef struct lobj { unsigned mark : 1, type : 4; char data[1]; } *lobj;

/*
  pargs: procedure arguments
  = evaluation_pattern + accept_rest_args? (1 bit) + arity (8 bits)
  example: .                                                    gfedcba r arity
  (lambda (a ,b c d ,e ,f . g) ...) -> pargs = 1111111111111111 1001101 1 0x06
*/

typedef int pargs;

/* lsubr: C function that can be called from LISP world */

typedef struct lsubr { pargs args; lobj (*function)(lobj); char* description; } lsubr;

/* --- macros --- */

extern unsigned int gc_protected, gc_protect_count;

#define WITH_GC_PROTECTION()                                            \
    for(gc_protected = 1; gc_protected; gc_protected = gc_protect_count = 0)

#define NIL NULL

/* defsubr */

#define DEFSUBR(name, args, rest)                       \
    lobj f_##name(lobj);                                \
    lsubr name = { ARGS(args, rest), f_##name, #name};  \
    lobj f_##name

#define Q (0)                   /* quote */
#define E (1)                   /* eval */
#define _ (2)                   /* none */

#define ARGS(PATTERN, REST)                             \
    ((ARGS_REST_E REST) << ((ARGS_COUNT(PATTERN)) + 9)) \
    | ((ARGS_PAT(PATTERN)) << 9)                        \
    | ((ARGS_REST_F REST) << 8)                         \
    | (ARGS_COUNT(PATTERN))

#define ARGS_REST_E(x) ARGS_REST_E##x
#define ARGS_REST_E0 0
#define ARGS_REST_E1 ~0
#define ARGS_REST_E2 0

#define ARGS_REST_F(x) ARGS_REST_F##x
#define ARGS_REST_F0 1
#define ARGS_REST_F1 1
#define ARGS_REST_F2 0

#define ARGS_COUNT(list) ARGS_COUNTA list (3)
#define ARGS_COUNTA(x) ARGS_COUNTA##x
#define ARGS_COUNTA0 1 + ARGS_COUNTB
#define ARGS_COUNTA1 1 + ARGS_COUNTB
#define ARGS_COUNTA2 0 + ARGS_COUNTB
#define ARGS_COUNTA3 0
#define ARGS_COUNTB(x) ARGS_COUNTB##x
#define ARGS_COUNTB0 1 + ARGS_COUNTA
#define ARGS_COUNTB1 1 + ARGS_COUNTA
#define ARGS_COUNTB2 0 + ARGS_COUNTA
#define ARGS_COUNTB3 0

#define ARGS_PAT(list) ARGS_PATLA list (3) ARGS_PATRA list (3)
#define ARGS_PATLA(x) ARGS_PATLA##x
#define ARGS_PATLA0 2 * ( ARGS_PATLB
#define ARGS_PATLA1 1 + 2 * ( ARGS_PATLB
#define ARGS_PATLA2 0 + ARGS_PATLB
#define ARGS_PATLA3 0
#define ARGS_PATLB(x) ARGS_PATLB##x
#define ARGS_PATLB0 2 * ( ARGS_PATLA
#define ARGS_PATLB1 1 + 2 * ( ARGS_PATLA
#define ARGS_PATLB2 0 + ARGS_PATLA
#define ARGS_PATLB3 0
#define ARGS_PATRA(x) ARGS_PATRA##x
#define ARGS_PATRA0 ) ARGS_PATRB
#define ARGS_PATRA1 ) ARGS_PATRB
#define ARGS_PATRA2 ARGS_PATRB
#define ARGS_PATRA3
#define ARGS_PATRB(x) ARGS_PATRB##x
#define ARGS_PATRB0 ) ARGS_PATRA
#define ARGS_PATRB1 ) ARGS_PATRA
#define ARGS_PATRB2 ARGS_PATRA
#define ARGS_PATRB3

/* --- lobj constructors --- */

lobj symbol();
lobj intern(char*);
int rintern(lobj, char*, unsigned);
lobj character(char);
lobj integer(int);
lobj floating(double);
lobj stream(FILE*);
lobj cons(lobj, lobj);
lobj make_array(unsigned, lobj);
lobj make_string(unsigned, char);
lobj function(pargs, lobj, lobj);
lobj closure(lobj, lobj);
lobj subr(lsubr);
lobj continuation(lobj);
lobj pa(pargs, lobj);

/* utilities */

lobj array(unsigned, ...);
lobj string(char*);
lobj list(unsigned, ...);

/* --- type predicates --- */

int symbolp(lobj);
int characterp(lobj);
int integerp(lobj);
int floatingp(lobj);
int streamp(lobj);
int consp(lobj);
int arrayp(lobj);
int stringp(lobj); /* may transform an array into a string if proper */
int functionp(lobj);
int closurep(lobj);
int subrp(lobj);
int continuationp(lobj);
int pap(lobj);

/* utilities */

int listp(lobj);

/* --- operations --- */

char character_value(lobj);
int integer_value(lobj);
double floating_value(lobj);
FILE* stream_value(lobj);
lobj car(lobj);
lobj cdr(lobj);
void setcar(lobj, lobj);
void setcdr(lobj, lobj);
unsigned array_length(lobj);
lobj* array_ptr(lobj);
unsigned (*string_length)(lobj);
char* (*string_ptr)(lobj);
void string_to_array(lobj);     /* transform a string into an array */
pargs function_args(lobj);
lobj function_formals(lobj);
lobj function_expr(lobj);
lobj (*closure_obj)(lobj);
lobj (*closure_env)(lobj);
pargs subr_args(lobj);
lobj (*subr_function(lobj))(lobj);
char* subr_description(lobj);
lobj continuation_callstack(lobj);
pargs pa_eval_pattern(lobj);
lobj pa_function(lobj);
void pa_set_function(lobj, lobj);
lobj pa_values(lobj);
int pa_num_values(lobj);
void pa_push(lobj, lobj);

/* ---------------- ---------------- ---------------- ---------------- */
#endif /* _PHI_H_ */

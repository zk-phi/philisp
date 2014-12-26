void subr_initialize();

/* nil */

lsubr subr_nilp;

/* symbol */

lsubr subr_symbolp;
lsubr subr_gensym;
lsubr subr_intern;

/* binding */

lsubr subr_bind;
lsubr subr_set;
lsubr subr_bound_value;

/* character */

lsubr subr_charp;
lsubr subr_char_to_int;
lsubr subr_int_to_char;

/* integer */

lsubr subr_integerp;

/* float */

lsubr subr_floatp;

/* number */

lsubr subr_mod;
lsubr subr_quot;
lsubr subr_round;
lsubr subr_add;
lsubr subr_mult;
lsubr subr_sub;
lsubr subr_div;
lsubr subr_le;
lsubr subr_lt;
lsubr subr_ge;
lsubr subr_gt;

/* stream */

lsubr subr_streamp;
lsubr subr_input_port;
lsubr subr_output_port;
lsubr subr_error_port;
lsubr subr_set_ports;
lsubr subr_getc;
lsubr subr_putc;
lsubr subr_ungetc;
lsubr subr_open;
lsubr subr_close;

/* cons */

lsubr subr_consp;
lsubr subr_cons;
lsubr subr_car;
lsubr subr_cdr;
lsubr subr_setcar;
lsubr subr_setcdr;

/* array */

lsubr subr_arrayp;
lsubr subr_make_array;
lsubr subr_aref;
lsubr subr_aset;
lsubr subr_stringp;

/* function */

lsubr subr_functionp;
lsubr subr_fn;

/* closure */

lsubr subr_closurep;
lsubr subr_closure;

/* subr */

lsubr subr_subrp;
lsubr subr_call_subr;
lsubr subr_dlsubr;

/* continuation */

lsubr subr_continuationp;

/* equality */

lsubr subr_eq;
lsubr subr_char_eq;
lsubr subr_num_eq;

/* parse/unparse */

lsubr subr_print;
lsubr subr_read;

/* evaluation */

lsubr subr_if;
lsubr subr_evlis;
lsubr subr_apply;
lsubr subr_unwind_protect;
lsubr subr_call_cc;
lsubr subr_eval;
lsubr subr_error;
lsubr subr_quote;

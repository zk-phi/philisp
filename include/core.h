#ifndef _CORE_H_
#define _CORE_H_ /* _CORE_H_ */

extern FILE *current_in, *current_out, *current_err;
extern char* last_parse_error;

void type_error(char*, unsigned, char*);
void lisp_error(char*);
void fatal(char*);

lobj binding(lobj, int);
void bind(lobj, lobj, int);
lobj save_current_env(int);
void restore_current_env(lobj);
void print(FILE*, lobj);
void stack_dump(FILE*);
lobj read();
lobj eval(lobj, lobj);
void core_initialize();

#endif /* _CORE_H_ */

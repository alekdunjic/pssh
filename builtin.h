#ifndef _builtin_h_
#define _builtin_h_

#include "parse.h"

int is_builtin (char* cmd);
void builtin_execute (Task T);
int builtin_which (Task T);
void set_fg_pgrp(pid_t pgrp);

#endif /* _builtin_h_ */

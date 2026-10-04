#ifndef MINISHELL_EXEC_H
#define MINISHELL_EXEC_H

#include "parser.h"

/*
 * Run a parsed pipeline. Foreground pipelines are waited for and the exit
 * status of the last command is returned (128 + signal number if it was
 * killed). Background pipelines return 0 immediately.
 */
int execute(const pipeline *p);

#endif

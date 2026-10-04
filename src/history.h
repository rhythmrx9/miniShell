#ifndef MINISHELL_HISTORY_H
#define MINISHELL_HISTORY_H

#include <stddef.h>

/* Remember a command line. Blank lines are ignored. */
void history_add(const char *line);

/* Print the last n entries (all of them if n is 0), numbered from 1. */
void history_print(size_t n);

void history_clear(void);

#endif

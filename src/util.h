#ifndef MINISHELL_UTIL_H
#define MINISHELL_UTIL_H

#include <stddef.h>

/* Allocation helpers that abort the shell on out-of-memory. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

#endif

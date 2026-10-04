#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void oom(void)
{
    fputs("minishell: out of memory\n", stderr);
    exit(2);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (!p)
        oom();
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);
    if (!p)
        oom();
    return p;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    return memcpy(xmalloc(len), s, len);
}

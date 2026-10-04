#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define HISTORY_MAX 1000

/* Ring buffer of the most recent HISTORY_MAX lines. */
static char *entries[HISTORY_MAX];
static size_t total; /* lines ever added; the oldest kept is total - count */
static size_t count;

void history_add(const char *line)
{
    if (line[strspn(line, " \t")] == '\0')
        return;

    size_t slot = total % HISTORY_MAX;
    free(entries[slot]);
    entries[slot] = xstrdup(line);
    total++;
    if (count < HISTORY_MAX)
        count++;
}

void history_print(size_t n)
{
    if (n == 0 || n > count)
        n = count;
    for (size_t i = total - n; i < total; i++)
        printf("%5zu  %s\n", i + 1, entries[i % HISTORY_MAX]);
}

void history_clear(void)
{
    for (size_t i = 0; i < HISTORY_MAX; i++) {
        free(entries[i]);
        entries[i] = NULL;
    }
    total = 0;
    count = 0;
}

#ifndef MINISHELL_JOBS_H
#define MINISHELL_JOBS_H

#include <stddef.h>
#include <sys/types.h>

/* Record a background job made of the given processes; returns its id. */
int jobs_add(const pid_t *pids, size_t n, const char *cmdline);

/* Collect reaped children and print a line for each job that finished. */
void jobs_notify(void);

#endif

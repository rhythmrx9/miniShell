#ifndef MINISHELL_SIGNALS_H
#define MINISHELL_SIGNALS_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

/*
 * Install the shell's handlers: SIGINT only sets a flag (so ^C never kills
 * the shell), SIGQUIT is ignored, and SIGCHLD reaps finished children.
 */
void signals_init(void);

/* Restore default dispositions in a freshly forked child. */
void signals_reset_child(const sigset_t *mask);

/* Block SIGCHLD, saving the previous mask in *prev. */
void signals_block_chld(sigset_t *prev);
void signals_restore_mask(const sigset_t *prev);

/* Return nonzero if SIGINT arrived since the last call, clearing the flag. */
int signals_take_interrupt(void);

/*
 * Move up to max children reaped by the SIGCHLD handler into pids/statuses.
 * Returns how many were copied.
 */
size_t signals_take_reaped(pid_t *pids, int *statuses, size_t max);

#endif

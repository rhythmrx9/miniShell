#ifndef MINISHELL_SHELL_H
#define MINISHELL_SHELL_H

/* State shared between the read loop and the built-ins. */
typedef struct {
    int last_status;    /* status of the most recent command ($?) */
    int exit_requested; /* set by the exit built-in */
} shell_state;

extern shell_state g_shell;

#endif

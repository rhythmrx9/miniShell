#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "exec.h"
#include "history.h"
#include "jobs.h"
#include "parser.h"
#include "shell.h"
#include "signals.h"
#include "tokenizer.h"

#define PROMPT "minishell$ "

shell_state g_shell;

static int run_line(const char *line, int last_status)
{
    token_list toks;
    pipeline p;

    history_add(line);
    if (tokenize(line, &toks) < 0)
        return 2;
    if (parse(&toks, &p) < 0) {
        token_list_free(&toks);
        return 2;
    }
    token_list_free(&toks);

    int status = p.ncmds ? execute(&p, line) : last_status;
    pipeline_free(&p);
    return status;
}

int main(void)
{
    int interactive = isatty(STDIN_FILENO);
    char *line = NULL;
    size_t cap = 0;

    signals_init();

    for (;;) {
        jobs_notify();
        signals_take_interrupt();
        if (interactive) {
            fputs(PROMPT, stdout);
            fflush(stdout);
        }

        errno = 0;
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0 && errno == EINTR) {
            /* ^C at the prompt: drop the line and prompt again. */
            clearerr(stdin);
            if (signals_take_interrupt()) {
                if (interactive)
                    putchar('\n');
                g_shell.last_status = 130;
            }
            continue;
        }
        if (n < 0) {
            if (interactive)
                putchar('\n');
            break;
        }
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        g_shell.last_status = run_line(line, g_shell.last_status);
        if (g_shell.exit_requested)
            break;
    }

    free(line);
    history_clear();
    return g_shell.last_status;
}

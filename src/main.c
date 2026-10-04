#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "exec.h"
#include "parser.h"
#include "tokenizer.h"

#define PROMPT "minishell$ "

static int run_line(const char *line, int last_status)
{
    token_list toks;
    pipeline p;

    if (tokenize(line, &toks) < 0)
        return 2;
    if (parse(&toks, &p) < 0) {
        token_list_free(&toks);
        return 2;
    }
    token_list_free(&toks);

    int status = p.ncmds ? execute(&p) : last_status;
    pipeline_free(&p);
    return status;
}

int main(void)
{
    int interactive = isatty(STDIN_FILENO);
    char *line = NULL;
    size_t cap = 0;
    int status = 0;

    for (;;) {
        if (interactive) {
            fputs(PROMPT, stdout);
            fflush(stdout);
        }

        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) {
            if (interactive)
                putchar('\n');
            break;
        }
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        status = run_line(line, status);
    }

    free(line);
    return status;
}

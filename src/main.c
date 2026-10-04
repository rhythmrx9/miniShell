#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define PROMPT "minishell$ "

int main(void)
{
    int interactive = isatty(STDIN_FILENO);
    char *line = NULL;
    size_t cap = 0;

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

        /* Command handling is added in later commits. */
    }

    free(line);
    return 0;
}

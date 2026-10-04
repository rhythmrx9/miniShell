#include "builtins.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "history.h"
#include "shell.h"

extern char **environ;

static size_t argc_of(char **argv)
{
    size_t n = 0;
    while (argv[n])
        n++;
    return n;
}

static int builtin_cd(char **argv)
{
    char oldpwd[PATH_MAX];
    char newpwd[PATH_MAX];
    const char *target;
    int print_dir = 0;

    if (argc_of(argv) > 2) {
        fputs("minishell: cd: too many arguments\n", stderr);
        return 1;
    }

    if (!argv[1]) {
        target = getenv("HOME");
        if (!target) {
            fputs("minishell: cd: HOME not set\n", stderr);
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) {
            fputs("minishell: cd: OLDPWD not set\n", stderr);
            return 1;
        }
        print_dir = 1;
    } else {
        target = argv[1];
    }

    int have_old = getcwd(oldpwd, sizeof oldpwd) != NULL;
    if (chdir(target) < 0) {
        fprintf(stderr, "minishell: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    if (have_old)
        setenv("OLDPWD", oldpwd, 1);
    if (getcwd(newpwd, sizeof newpwd)) {
        setenv("PWD", newpwd, 1);
        if (print_dir)
            puts(newpwd);
    }
    return 0;
}

static int builtin_pwd(char **argv)
{
    char cwd[PATH_MAX];

    (void)argv;
    if (!getcwd(cwd, sizeof cwd)) {
        fprintf(stderr, "minishell: pwd: %s\n", strerror(errno));
        return 1;
    }
    puts(cwd);
    return 0;
}

static int valid_name(const char *s, size_t len)
{
    if (len == 0 || !(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for (size_t i = 1; i < len; i++)
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_'))
            return 0;
    return 1;
}

static int builtin_export(char **argv)
{
    int status = 0;

    if (!argv[1]) {
        for (char **e = environ; *e; e++) {
            const char *eq = strchr(*e, '=');
            if (eq)
                printf("export %.*s=\"%s\"\n", (int)(eq - *e), *e, eq + 1);
        }
        return 0;
    }

    for (size_t i = 1; argv[i]; i++) {
        char *eq = strchr(argv[i], '=');
        size_t name_len = eq ? (size_t)(eq - argv[i]) : strlen(argv[i]);

        if (!valid_name(argv[i], name_len)) {
            fprintf(stderr, "minishell: export: `%s': not a valid identifier\n",
                    argv[i]);
            status = 1;
            continue;
        }
        /* Every variable in our environment is already exported. */
        if (!eq)
            continue;

        *eq = '\0';
        if (setenv(argv[i], eq + 1, 1) < 0) {
            fprintf(stderr, "minishell: export: %s\n", strerror(errno));
            status = 1;
        }
        *eq = '=';
    }
    return status;
}

static int parse_count(const char *s, long *out)
{
    char *end;

    errno = 0;
    *out = strtol(s, &end, 10);
    return *s != '\0' && *end == '\0' && errno == 0;
}

static int builtin_history(char **argv)
{
    long n = 0;

    if (argv[1] && strcmp(argv[1], "-c") == 0) {
        history_clear();
        return 0;
    }
    if (argv[1] && (!parse_count(argv[1], &n) || n < 0)) {
        fprintf(stderr, "minishell: history: %s: numeric argument required\n",
                argv[1]);
        return 1;
    }
    history_print((size_t)n);
    return 0;
}

static int builtin_exit(char **argv)
{
    long code = g_shell.last_status;

    if (argv[1]) {
        if (!parse_count(argv[1], &code)) {
            fprintf(stderr, "minishell: exit: %s: numeric argument required\n",
                    argv[1]);
            code = 2;
        } else if (argv[2]) {
            fputs("minishell: exit: too many arguments\n", stderr);
            return 1;
        }
    }
    g_shell.exit_requested = 1;
    return (int)(code & 0xff);
}

static const struct {
    const char *name;
    builtin_fn fn;
} builtins[] = {
    {"cd", builtin_cd},
    {"pwd", builtin_pwd},
    {"export", builtin_export},
    {"history", builtin_history},
    {"exit", builtin_exit},
};

builtin_fn builtin_lookup(const char *name)
{
    for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
        if (strcmp(builtins[i].name, name) == 0)
            return builtins[i].fn;
    return NULL;
}

#ifndef MINISHELL_BUILTINS_H
#define MINISHELL_BUILTINS_H

typedef int (*builtin_fn)(char **argv);

/* Return the built-in named name, or NULL if it is an external command. */
builtin_fn builtin_lookup(const char *name);

#endif

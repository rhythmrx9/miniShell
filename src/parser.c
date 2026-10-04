#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static const char *token_name(const token *t)
{
    if (!t)
        return "newline";
    switch (t->type) {
    case TOK_PIPE:         return "|";
    case TOK_REDIR_IN:     return "<";
    case TOK_REDIR_OUT:    return ">";
    case TOK_REDIR_APPEND: return ">>";
    case TOK_AMP:          return "&";
    case TOK_WORD:         return t->text;
    }
    return "?";
}

static void syntax_error(const token *t)
{
    fprintf(stderr, "minishell: syntax error near unexpected token `%s'\n",
            token_name(t));
}

static command *new_command(pipeline *p)
{
    p->cmds = xrealloc(p->cmds, (p->ncmds + 1) * sizeof *p->cmds);
    command *c = &p->cmds[p->ncmds++];
    memset(c, 0, sizeof *c);
    c->argv = xmalloc(sizeof *c->argv);
    c->argv[0] = NULL;
    return c;
}

static void push_arg(command *c, const char *arg)
{
    c->argv = xrealloc(c->argv, (c->argc + 2) * sizeof *c->argv);
    c->argv[c->argc++] = xstrdup(arg);
    c->argv[c->argc] = NULL;
}

int parse(const token_list *toks, pipeline *out)
{
    memset(out, 0, sizeof *out);
    if (toks->len == 0)
        return 0;

    command *cur = new_command(out);

    for (size_t i = 0; i < toks->len; i++) {
        const token *t = &toks->items[i];
        const token *next = i + 1 < toks->len ? &toks->items[i + 1] : NULL;

        switch (t->type) {
        case TOK_WORD:
            push_arg(cur, t->text);
            break;

        case TOK_PIPE:
            if (cur->argc == 0 || !next || next->type == TOK_AMP) {
                syntax_error(cur->argc == 0 ? t : next);
                goto fail;
            }
            cur = new_command(out);
            break;

        case TOK_REDIR_IN:
        case TOK_REDIR_OUT:
        case TOK_REDIR_APPEND:
            if (!next || next->type != TOK_WORD) {
                syntax_error(next);
                goto fail;
            }
            if (t->type == TOK_REDIR_IN) {
                free(cur->infile);
                cur->infile = xstrdup(next->text);
            } else {
                free(cur->outfile);
                cur->outfile = xstrdup(next->text);
                cur->append = t->type == TOK_REDIR_APPEND;
            }
            i++;
            break;

        case TOK_AMP:
            if (cur->argc == 0 || next) {
                syntax_error(cur->argc == 0 ? t : next);
                goto fail;
            }
            out->background = 1;
            break;
        }
    }

    if (cur->argc == 0) {
        /* Only redirections, e.g. "> file". */
        fputs("minishell: syntax error: missing command\n", stderr);
        goto fail;
    }
    return 0;

fail:
    pipeline_free(out);
    return -1;
}

void pipeline_free(pipeline *p)
{
    for (size_t i = 0; i < p->ncmds; i++) {
        command *c = &p->cmds[i];
        for (size_t j = 0; j < c->argc; j++)
            free(c->argv[j]);
        free(c->argv);
        free(c->infile);
        free(c->outfile);
    }
    free(p->cmds);
    memset(p, 0, sizeof *p);
}

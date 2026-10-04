#include "tokenizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} strbuf;

static void sb_push(strbuf *sb, char c)
{
    if (sb->len + 1 >= sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 32;
        sb->buf = xrealloc(sb->buf, sb->cap);
    }
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

static char *sb_finish(strbuf *sb)
{
    return sb->buf ? sb->buf : xstrdup("");
}

static void push_token(token_list *list, token_type type, char *text)
{
    if (list->len == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = xrealloc(list->items, list->cap * sizeof *list->items);
    }
    list->items[list->len].type = type;
    list->items[list->len].text = text;
    list->len++;
}

static int is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_operator(char c)
{
    return c == '|' || c == '<' || c == '>' || c == '&';
}

/* Read one word starting at line[*pos]; advances *pos past it. */
static int read_word(const char *line, size_t *pos, strbuf *sb)
{
    size_t i = *pos;

    while (line[i] && !is_blank(line[i]) && !is_operator(line[i])) {
        char c = line[i];

        if (c == '\'') {
            i++;
            while (line[i] && line[i] != '\'')
                sb_push(sb, line[i++]);
            if (!line[i]) {
                fputs("minishell: syntax error: unterminated single quote\n", stderr);
                return -1;
            }
            i++;
        } else if (c == '"') {
            i++;
            while (line[i] && line[i] != '"') {
                if (line[i] == '\\' && line[i + 1] && strchr("\"\\$", line[i + 1]))
                    i++;
                sb_push(sb, line[i++]);
            }
            if (!line[i]) {
                fputs("minishell: syntax error: unterminated double quote\n", stderr);
                return -1;
            }
            i++;
        } else if (c == '\\') {
            i++;
            if (line[i])
                sb_push(sb, line[i++]);
            else
                sb_push(sb, '\\');
        } else {
            sb_push(sb, line[i++]);
        }
    }

    *pos = i;
    return 0;
}

int tokenize(const char *line, token_list *out)
{
    size_t i = 0;

    memset(out, 0, sizeof *out);

    while (line[i]) {
        char c = line[i];

        if (is_blank(c)) {
            i++;
        } else if (c == '#') {
            break;
        } else if (c == '|') {
            push_token(out, TOK_PIPE, NULL);
            i++;
        } else if (c == '&') {
            push_token(out, TOK_AMP, NULL);
            i++;
        } else if (c == '<') {
            push_token(out, TOK_REDIR_IN, NULL);
            i++;
        } else if (c == '>') {
            if (line[i + 1] == '>') {
                push_token(out, TOK_REDIR_APPEND, NULL);
                i += 2;
            } else {
                push_token(out, TOK_REDIR_OUT, NULL);
                i++;
            }
        } else {
            strbuf sb = {0};
            if (read_word(line, &i, &sb) < 0) {
                free(sb.buf);
                token_list_free(out);
                return -1;
            }
            push_token(out, TOK_WORD, sb_finish(&sb));
        }
    }

    return 0;
}

void token_list_free(token_list *list)
{
    for (size_t i = 0; i < list->len; i++)
        free(list->items[i].text);
    free(list->items);
    memset(list, 0, sizeof *list);
}

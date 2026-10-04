#ifndef MINISHELL_TOKENIZER_H
#define MINISHELL_TOKENIZER_H

#include <stddef.h>

typedef enum {
    TOK_WORD,
    TOK_PIPE,         /* |  */
    TOK_REDIR_IN,     /* <  */
    TOK_REDIR_OUT,    /* >  */
    TOK_REDIR_APPEND, /* >> */
    TOK_AMP,          /* &  */
} token_type;

typedef struct {
    token_type type;
    char *text; /* owned; set for TOK_WORD only */
} token;

typedef struct {
    token *items;
    size_t len;
    size_t cap;
} token_list;

/*
 * Split a command line into words and operators.
 *
 * Whitespace separates words. Single quotes preserve everything literally,
 * double quotes allow \" \\ \$ escapes, and a backslash outside quotes
 * escapes the next character. Adjacent quoted and unquoted pieces join into
 * one word. An unquoted '#' at the start of a word begins a comment.
 *
 * Returns 0 on success, or -1 on a syntax error after printing a message.
 */
int tokenize(const char *line, token_list *out);
void token_list_free(token_list *list);

#endif

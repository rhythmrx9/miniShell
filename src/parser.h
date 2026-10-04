#ifndef MINISHELL_PARSER_H
#define MINISHELL_PARSER_H

#include <stddef.h>

#include "tokenizer.h"

typedef struct {
    char **argv;   /* NULL-terminated */
    size_t argc;
    char *infile;  /* < file, or NULL */
    char *outfile; /* > or >> file, or NULL */
    int append;    /* outfile was given with >> */
} command;

typedef struct {
    command *cmds;
    size_t ncmds;   /* 0 for an empty line */
    int background; /* line ended with & */
} pipeline;

/*
 * Build a pipeline from tokens:
 *
 *   pipeline := command ('|' command)* ['&']
 *   command  := (WORD | '<' WORD | '>' WORD | '>>' WORD)+
 *
 * Returns 0 on success, or -1 on a syntax error after printing a message.
 */
int parse(const token_list *toks, pipeline *out);
void pipeline_free(pipeline *p);

#endif

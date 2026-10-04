# miniShell

A small Unix command-line shell written in C. It has its own tokenizer that
handles quoting and whitespace, and it runs multi-stage pipelines with
input/output redirection and background jobs, using only POSIX system calls.

```
$ ./minishell
minishell$ export GREETING="hello world"
minishell$ echo "$GREETING" | tr a-z A-Z > shout.txt
minishell$ cat < shout.txt
HELLO WORLD
minishell$ sleep 2 &
[1] 48213
minishell$ history 2
    4  sleep 2 &
    5  history 2
minishell$
[1]  Done		sleep 2 &
minishell$ exit
```

## Building

Requires a C11 compiler and `make` on Linux or macOS.

```sh
make            # builds ./minishell
make test       # runs the end-to-end test suite
make clean
```

## Features

| Area | Supported |
| --- | --- |
| Quoting | `'single'` (literal), `"double"` (with `\"` `\\` `\$` escapes), `\` escapes, adjacent pieces join (`a"b c"'d'` → `ab cd`) |
| Expansion | `$NAME` and `$?` outside single quotes |
| Pipelines | any number of stages: `cmd1 \| cmd2 \| cmd3` |
| Redirection | `< file`, `> file`, `>> file`, on any stage |
| Background | a trailing `&` runs the pipeline as a job; a `Done` line is printed when it finishes |
| Built-ins | `cd [dir \| -]`, `pwd`, `export [NAME=value ...]`, `history [N \| -c]`, `exit [status]` |
| Signals | `^C` kills the running command but never the shell; at the prompt it clears the line |
| Comments | `#` at the start of a word starts a comment |

Exit statuses follow the usual shell conventions: 127 for an unknown command,
126 when a command can't be executed, 128+N when a command is killed by signal N,
and 2 for a syntax error.

## How it works

```
line ─▶ tokenizer ─▶ parser ─▶ executor ─▶ fork / execvp / waitpid
         (words,      (pipeline of            pipe + dup2 between stages,
          operators)   commands + redirs)     open + dup2 for redirections
```

| File | Responsibility |
| --- | --- |
| `src/main.c` | Read loop: prompt, `getline`, history, dispatch, `^C` at the prompt |
| `src/tokenizer.c` | Splits a line into words and the `\|` `<` `>` `>>` `&` operators; quoting and `$` expansion |
| `src/parser.c` | Builds a `pipeline` of `command`s (argv + redirections) and reports syntax errors |
| `src/exec.c` | Forks one child per stage, connects them with `pipe`/`dup2`, applies redirections, waits |
| `src/builtins.c` | `cd`, `pwd`, `export`, `history`, `exit` |
| `src/signals.c` | `sigaction` setup: SIGINT flag, SIGQUIT ignored, SIGCHLD reaper |
| `src/jobs.c` | Background job table and completion notices |
| `src/history.c` | Ring buffer of the last 1000 command lines |

Some design details:

- **Built-ins in the shell process.** A built-in on its own (e.g. `cd /tmp`)
  runs inside the shell so that it can change the shell's directory,
  environment or exit it. Its redirections are applied with `dup2` and undone
  afterwards. In a pipeline or in the background a built-in runs in the forked
  child like any other command, so `exit | true` does not exit the shell.
- **Reaping without races.** SIGCHLD is blocked while a foreground pipeline is
  forked and waited for with `waitpid`. That means the SIGCHLD handler, which
  calls `waitpid(-1, ..., WNOHANG)`, only ever reaps background children. It
  records them in a fixed-size buffer, and the main loop turns those records
  into `Done` lines before the next prompt.
- **Interrupts.** The SIGINT handler only sets a flag. It is installed without
  `SA_RESTART`, so `^C` interrupts the blocking `getline` at the prompt. Children
  restore default dispositions before `exec`, so `^C` terminates the foreground
  command.
- **Background jobs** get their own process group (`setpgid`), so a `^C` typed
  at the terminal doesn't reach them. Their stdin comes from `/dev/null` unless
  it is redirected.

## Limitations

This is a minimal shell, not a POSIX `sh` replacement. It does not support:
`;`, `&&`, `||`, subshells, globbing, here-documents, `2>` or other fd-numbered
redirections, field splitting of expanded variables, or job control
(`fg`/`bg`/`jobs`, `^Z`).

#include "exec.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "jobs.h"
#include "signals.h"
#include "util.h"

static int open_redirect(const char *path, int flags, int target_fd)
{
    int fd = open(path, flags, 0644);
    if (fd < 0) {
        fprintf(stderr, "minishell: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fd != target_fd) {
        if (dup2(fd, target_fd) < 0) {
            fprintf(stderr, "minishell: dup2: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

/* Point stdin/stdout at the command's redirection targets. */
static int apply_redirections(const command *c)
{
    if (c->infile && open_redirect(c->infile, O_RDONLY, STDIN_FILENO) < 0)
        return -1;
    if (c->outfile) {
        int flags = O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC);
        if (open_redirect(c->outfile, flags, STDOUT_FILENO) < 0)
            return -1;
    }
    return 0;
}

static int wait_status_to_code(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGINT)
            fputc('\n', stderr);
        else if (sig != SIGPIPE)
            fprintf(stderr, "%s\n", strsignal(sig));
        return 128 + sig;
    }
    return 1;
}

/* Runs in the forked child; never returns. */
static void run_child(const pipeline *p, size_t idx, int in_fd, int out_fd,
                      pid_t pgid, const sigset_t *mask)
{
    const command *c = &p->cmds[idx];

    signals_reset_child(mask);

    if (p->background) {
        /* Own process group so terminal signals don't reach the job. */
        setpgid(0, pgid);
        if (idx == 0 && !c->infile)
            open_redirect("/dev/null", O_RDONLY, STDIN_FILENO);
    }

    if (in_fd != STDIN_FILENO) {
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
    }
    if (apply_redirections(c) < 0)
        _exit(1);

    builtin_fn builtin = builtin_lookup(c->argv[0]);
    if (builtin) {
        int status = builtin(c->argv);
        fflush(stdout);
        _exit(status);
    }

    execvp(c->argv[0], c->argv);
    int err = errno;
    if (err == ENOENT)
        fprintf(stderr, "minishell: %s: command not found\n", c->argv[0]);
    else
        fprintf(stderr, "minishell: %s: %s\n", c->argv[0], strerror(err));
    _exit(err == ENOENT ? 127 : 126);
}

/*
 * Run a built-in inside the shell process so it can change the shell's own
 * state (cwd, environment, exit). Redirections are undone afterwards.
 */
static int run_builtin_in_shell(const command *c, builtin_fn builtin)
{
    int saved_in = -1, saved_out = -1;
    int status;

    fflush(stdout);
    if (c->infile)
        saved_in = dup(STDIN_FILENO);
    if (c->outfile)
        saved_out = dup(STDOUT_FILENO);

    if (apply_redirections(c) < 0)
        status = 1;
    else
        status = builtin(c->argv);

    fflush(stdout);
    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    return status;
}

int execute(const pipeline *p, const char *cmdline)
{
    if (p->ncmds == 0)
        return 0;

    if (p->ncmds == 1 && !p->background) {
        builtin_fn builtin = builtin_lookup(p->cmds[0].argv[0]);
        if (builtin)
            return run_builtin_in_shell(&p->cmds[0], builtin);
    }

    pid_t *pids = xmalloc(p->ncmds * sizeof *pids);
    size_t started = 0;
    pid_t pgid = 0;
    int prev_read = STDIN_FILENO;
    int status = 0;
    sigset_t prev_mask;

    /* Don't let buffered output get duplicated into the children. */
    fflush(NULL);

    /*
     * Hold SIGCHLD until every foreground child is waited for, so the
     * handler only ever reaps background jobs.
     */
    signals_block_chld(&prev_mask);

    for (size_t i = 0; i < p->ncmds; i++) {
        int fds[2] = {-1, -1};
        int is_last = i + 1 == p->ncmds;

        if (!is_last && pipe(fds) < 0) {
            fprintf(stderr, "minishell: pipe: %s\n", strerror(errno));
            status = 1;
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "minishell: fork: %s\n", strerror(errno));
            if (!is_last) {
                close(fds[0]);
                close(fds[1]);
            }
            status = 1;
            break;
        }
        if (pid == 0) {
            if (!is_last)
                close(fds[0]);
            run_child(p, i, prev_read, is_last ? STDOUT_FILENO : fds[1], pgid,
                      &prev_mask);
        }

        if (p->background) {
            /* Set the group from both sides to avoid racing the child. */
            setpgid(pid, pgid ? pgid : pid);
            if (!pgid)
                pgid = pid;
        }

        pids[started++] = pid;
        if (prev_read != STDIN_FILENO)
            close(prev_read);
        if (!is_last) {
            close(fds[1]);
            prev_read = fds[0];
        }
    }
    if (prev_read != STDIN_FILENO)
        close(prev_read);

    if (p->background) {
        if (started > 0) {
            int id = jobs_add(pids, started, cmdline);
            printf("[%d] %d\n", id, (int)pids[started - 1]);
        }
    } else {
        for (size_t i = 0; i < started; i++) {
            int wstatus;
            while (waitpid(pids[i], &wstatus, 0) < 0) {
                if (errno != EINTR) {
                    wstatus = 0;
                    break;
                }
            }
            if (i + 1 == p->ncmds)
                status = wait_status_to_code(wstatus);
        }
    }

    signals_restore_mask(&prev_mask);
    free(pids);
    return status;
}

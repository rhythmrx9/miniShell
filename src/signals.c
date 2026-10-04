#include "signals.h"

#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#define REAPED_MAX 256

static volatile sig_atomic_t got_sigint;

/* Written only by the SIGCHLD handler; read with SIGCHLD blocked. */
static pid_t reaped_pids[REAPED_MAX];
static int reaped_statuses[REAPED_MAX];
static volatile sig_atomic_t reaped_count;

static void on_sigint(int sig)
{
    (void)sig;
    got_sigint = 1;
}

static void on_sigchld(int sig)
{
    int saved_errno = errno;
    pid_t pid;
    int status;

    (void)sig;
    /*
     * Foreground children are waited for with SIGCHLD blocked, so anything
     * reaped here belongs to a background job.
     */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (reaped_count < REAPED_MAX) {
            reaped_pids[reaped_count] = pid;
            reaped_statuses[reaped_count] = status;
            reaped_count++;
        }
    }
    errno = saved_errno;
}

static void set_handler(int sig, void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sa.sa_flags = flags;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void signals_init(void)
{
    /* No SA_RESTART: ^C should interrupt a blocking read at the prompt. */
    set_handler(SIGINT, on_sigint, 0);
    set_handler(SIGQUIT, SIG_IGN, 0);
    set_handler(SIGCHLD, on_sigchld, SA_RESTART | SA_NOCLDSTOP);
}

void signals_reset_child(const sigset_t *mask)
{
    set_handler(SIGINT, SIG_DFL, 0);
    set_handler(SIGQUIT, SIG_DFL, 0);
    set_handler(SIGCHLD, SIG_DFL, 0);
    /*
     * The shell may have been started with SIGPIPE ignored (CI runners do
     * this); a pipeline stage whose reader went away should still just die.
     */
    set_handler(SIGPIPE, SIG_DFL, 0);
    sigprocmask(SIG_SETMASK, mask, NULL);
}

void signals_block_chld(sigset_t *prev)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, prev);
}

void signals_restore_mask(const sigset_t *prev)
{
    sigprocmask(SIG_SETMASK, prev, NULL);
}

int signals_take_interrupt(void)
{
    int was_set = got_sigint;
    got_sigint = 0;
    return was_set;
}

size_t signals_take_reaped(pid_t *pids, int *statuses, size_t max)
{
    sigset_t prev;
    size_t n;

    signals_block_chld(&prev);
    n = (size_t)reaped_count < max ? (size_t)reaped_count : max;
    memcpy(pids, reaped_pids, n * sizeof *pids);
    memcpy(statuses, reaped_statuses, n * sizeof *statuses);
    /* Keep any entries that didn't fit for the next call. */
    memmove(reaped_pids, reaped_pids + n, (reaped_count - n) * sizeof *pids);
    memmove(reaped_statuses, reaped_statuses + n,
            (reaped_count - n) * sizeof *statuses);
    reaped_count -= n;
    signals_restore_mask(&prev);
    return n;
}

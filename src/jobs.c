#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "signals.h"
#include "util.h"

typedef struct job {
    int id;
    pid_t *pids;  /* entries set to 0 once reaped */
    size_t npids;
    size_t remaining;
    int last_status; /* wait status of the final pipeline stage */
    char *cmdline;
    struct job *next;
} job;

static job *jobs;

int jobs_add(const pid_t *pids, size_t n, const char *cmdline)
{
    job *j = xmalloc(sizeof *j);
    int id = 1;

    for (job *it = jobs; it; it = it->next)
        if (it->id >= id)
            id = it->id + 1;

    j->id = id;
    j->pids = xmalloc(n * sizeof *pids);
    memcpy(j->pids, pids, n * sizeof *pids);
    j->npids = n;
    j->remaining = n;
    j->last_status = 0;
    j->cmdline = xstrdup(cmdline);

    /* Append so jobs are reported in launch order. */
    job **tail = &jobs;
    while (*tail)
        tail = &(*tail)->next;
    j->next = NULL;
    *tail = j;
    return id;
}

static void mark_reaped(pid_t pid, int status)
{
    for (job *j = jobs; j; j = j->next) {
        for (size_t i = 0; i < j->npids; i++) {
            if (j->pids[i] == pid) {
                j->pids[i] = 0;
                j->remaining--;
                if (i + 1 == j->npids)
                    j->last_status = status;
                return;
            }
        }
    }
}

static void print_done(const job *j)
{
    int st = j->last_status;

    if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
        printf("[%d]  Done\t\t%s\n", j->id, j->cmdline);
    else if (WIFEXITED(st))
        printf("[%d]  Exit %d\t\t%s\n", j->id, WEXITSTATUS(st), j->cmdline);
    else
        printf("[%d]  %s\t\t%s\n", j->id, strsignal(WTERMSIG(st)), j->cmdline);
}

void jobs_notify(void)
{
    pid_t pids[64];
    int statuses[64];
    size_t n;

    while ((n = signals_take_reaped(pids, statuses, 64)) > 0)
        for (size_t i = 0; i < n; i++)
            mark_reaped(pids[i], statuses[i]);

    job **link = &jobs;
    while (*link) {
        job *j = *link;
        if (j->remaining == 0) {
            print_done(j);
            *link = j->next;
            free(j->pids);
            free(j->cmdline);
            free(j);
        } else {
            link = &j->next;
        }
    }
    fflush(stdout);
}

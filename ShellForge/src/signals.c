#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <signal.h>
#include <stdio.h>

static void shell_sigchld_handler(int sig)
{
    (void)sig;
}

void shell_block_jobcontrol_signals(sigset_t *oldmask)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTTOU);
    sigaddset(&mask, SIGTTIN);
    sigaddset(&mask, SIGTSTP);
    if (sigprocmask(SIG_BLOCK, &mask, oldmask) != 0)
        perror("sigprocmask");
}

void shell_restore_jobcontrol_signals(const sigset_t *oldmask)
{
    if (oldmask == NULL)
        return;
    if (sigprocmask(SIG_SETMASK, oldmask, NULL) != 0)
        perror("sigprocmask");
}

void shell_signal_setup(void)
{
    struct sigaction ignore = {0};
    struct sigaction sigchld = {0};

    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    sigaction(SIGINT, &ignore, NULL);
    sigaction(SIGQUIT, &ignore, NULL);
    sigaction(SIGTSTP, &ignore, NULL);
    sigaction(SIGTTIN, &ignore, NULL);
    sigaction(SIGTTOU, &ignore, NULL);

    sigchld.sa_handler = shell_sigchld_handler;
    sigemptyset(&sigchld.sa_mask);
    sigchld.sa_flags = 0;
    sigaction(SIGCHLD, &sigchld, NULL);
}

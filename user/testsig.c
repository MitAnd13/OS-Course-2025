#include <inc/lib.h>

static volatile int got_usr;

static void
usr1_handler(int sig, siginfo_t *info, void *ctx) {
    USED(sig);
    USED(ctx);
    got_usr = 1;
}

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);
    
    cprintf("testsig: starting\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sigact_action = usr1_handler;
    sa.sigact_flags = SA_SIGINFO;
    sigemptyset(&sa.sigact_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        panic("sigaction failed");

    if (sigkill(0, SIGUSR1) < 0)
        panic("sigkill failed");

    for (int i = 0; i < 10000 && !got_usr; i++)
        sys_yield();

    if (!got_usr)
        panic("SIGUSR1 handler did not run");

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    envid_t parent = sys_getenvid();
    envid_t child = fork();
    if (child < 0)
        panic("fork failed");
    if (child == 0) {
        sigkill(parent, SIGUSR2);
        exit();
    }

    int sig = 0;
    
    if (sigwait(&set, &sig) < 0)
        panic("sigwait failed");
    if (sig != SIGUSR2)
        panic("sigwait returned wrong signal");

    wait(child);
    cprintf("signals: OK\n");
}

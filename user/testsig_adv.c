#include <inc/lib.h>

static volatile int got_cnt;
static volatile int last_sig;

static void
handler_siginfo(int sig, siginfo_t *info, void *ctx) {
    USED(ctx);
    got_cnt++;
    last_sig = sig;
}

static void
handler_plain(int sig) {
    got_cnt++;
    last_sig = sig;
}

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);

    cprintf("testsig_adv: starting\n");

    struct sigaction sa1, sa2, old;

    memset(&sa1, 0, sizeof(sa1));
    sa1.sigact_handler = handler_plain;
    sigemptyset(&sa1.sigact_mask);

    if (sigaction(SIGUSR1, &sa1, NULL) < 0)
        panic("sigaction sa1 failed");

    memset(&sa2, 0, sizeof(sa2));
    sa2.sigact_action = handler_siginfo;
    sa2.sigact_flags = SA_SIGINFO;
    sigemptyset(&sa2.sigact_mask);

    if (sigaction(SIGUSR1, &sa2, &old) < 0)
        panic("sigaction sa2 failed");

    if (old.sigact_handler != sa1.sigact_handler || (old.sigact_flags & SA_SIGINFO))
        panic("sigaction did not return previous handler correctly");

    got_cnt = 0;

    if (sigkill(0, SIGUSR1) < 0)
        panic("sigkilling self failed");

    for (int i = 0; i < 2000 && got_cnt < 1; i++)
        sys_yield();

    if (got_cnt != 1 || last_sig != SIGUSR1)
        panic("handler got wrong signal");

    struct sigaction sa_usr2;
    memset(&sa_usr2, 0, sizeof(sa_usr2));
    sa_usr2.sigact_handler = handler_plain;
    sigemptyset(&sa_usr2.sigact_mask);
    sigaction(SIGUSR2, &sa_usr2, NULL);

    sys_sigentry(NULL);
    
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    sigkill(0, SIGUSR1);
    sigkill(0, SIGUSR2);
    sigkill(0, SIGUSR1);

    int sig;
    sigwait(&set, &sig);
    if (sig != SIGUSR1)
        panic("sigwait #1 expected SIGUSR1");

    sigwait(&set, &sig);
    if (sig != SIGUSR2)
        panic("sigwait #2 expected SIGUSR2");

    sigwait(&set, &sig);
    if (sig != SIGUSR1)
        panic("sigwait #3 expected SIGUSR1");

    sig_restore_entry();

    envid_t child = fork();
    if (child == 0) {
        sys_sigentry(NULL);
        ipc_recv(NULL, NULL, NULL, NULL);
        exit();
    }

    int ok = 0;
    for (int i = 0; i < SIG_QUEUE_MAX; i++) {
        if (sigkill(child, SIGUSR1) == 0)
            ok++;
    }

    if (ok != SIG_QUEUE_MAX)
        panic("couldn't fill signal queue");

    int r = sigkill(child, SIGUSR1);
    if (r != -E_NO_MEM)
        panic("expected -E_NO_MEM on signal queue overflow");

    ipc_send(child, 0, NULL, 0, 0);
    wait(child);

    if (sigaction(SIGKILL, &sa1, NULL) != -E_INVAL)
        panic("sigaction(SIGKILL) should fail");

    cprintf("testsig_adv: OK\n");
}

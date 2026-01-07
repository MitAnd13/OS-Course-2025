// Тест утилиты kill.
// Проверяет, что:
//  - kill посылает сигнал указанному envid

#include <inc/lib.h>

static volatile int got_sig;

static void
handler(int sig, siginfo_t *info, void *ctx) {
    USED(ctx);
    got_sig = sig;
}

void
umain(int argc, char **argv) {
    USED(argc);
    USED(argv);

    cprintf("testkill: starting\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sigact_action = handler;
    sa.sigact_flags = SA_SIGINFO;
    sigemptyset(&sa.sigact_mask);
    sigaction(SIGUSR1, &sa, NULL);

    envid_t parent = sys_getenvid();
    envid_t child = fork();

    if (child == 0) {
        char sigbuf[16], envidbuf[32];
        snprintf(sigbuf, sizeof(sigbuf), "-%d", SIGUSR1);
        snprintf(envidbuf, sizeof(envidbuf), "0x%x", parent);

        spawnl("/kill", "kill", sigbuf, envidbuf, 0);
        exit();
    }

    for (int i = 0; i < 32000 && got_sig == 0; i++)
        sys_yield();

    if (got_sig != SIGUSR1)
        panic("kill delivered wrong signal");

    wait(child);
    cprintf("testkill: OK\n");
}

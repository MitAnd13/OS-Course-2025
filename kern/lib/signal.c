#include <inc/lib.h>

static bool sig_inited;
static envid_t sig_inited_env;

static void __attribute__((noreturn))
sig_entry(struct Sigframe *frame) {
    if (frame->sf_flags & SA_SIGINFO) {
        void (*handler)(int, siginfo_t *, void *) = frame->sf_handler;
        handler(frame->sf_info.siginfo_num, &frame->sf_info, &frame->sf_tf);
    } else {
        void (*handler)(int) = frame->sf_handler;
        handler(frame->sf_info.siginfo_num);
    }

    sys_sigreturn(frame);
    panic("sigreturn failed");
}

static void
sig_init(void) {
    envid_t cur = sys_getenvid();
    if (sig_inited && sig_inited_env == cur) return;
    if (sys_sigentry(sig_entry) < 0) {
        panic("sigentry setup failed");
    }
    sig_inited = 1;
    sig_inited_env = cur;
}

void
sig_restore_entry(void) {
    if (sys_sigentry(sig_entry) < 0) {
        panic("sigentry restore failed");
    }
}

int
sigqueue(envid_t pid, int signo, const sigval_t value) {
    return sys_sigqueue(pid, signo, value);
}

int
sigwait(const sigset_t *set, int *sig) {
    return sys_sigwait(set, sig);
}

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    if (act) sig_init();
    return sys_sigaction(sig, act, oact);
}

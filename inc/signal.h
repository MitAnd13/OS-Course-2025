#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <inc/types.h>
#include <inc/trap.h>

typedef uint64_t sigset_t;

typedef union sigval {
    int sigval_int;
    void *sigval_ptr;
} sigval_t;

typedef struct siginfo {
    int siginfo_num;
    sigval_t siginfo_val;
} siginfo_t;

struct sigaction {
    union {
        void (*sigact_handler)(int);
        void (*sigact_action)(int, siginfo_t *, void *);
    };
    sigset_t sigact_mask;
    int sigact_flags;
};

#define SA_SIGINFO 0x1

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

#define SIGHUP  1
#define SIGINT  2
#define SIGQUIT 3
#define SIGILL  4
#define SIGABRT 6
#define SIGFPE  8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15

#define NSIG 32
#define SIG_QUEUE_MAX 32

static inline int
sig_valid(int sig) {
    return sig > 0 && sig < NSIG;
}

static inline sigset_t
sig_bit(int sig) {
    return 1ULL << (sig - 1);
}

static inline void
sigemptyset(sigset_t *set) {
    *set = 0;
}

static inline void
sigfillset(sigset_t *set) {
    *set = ~0ULL;
}

static inline int
sigaddset(sigset_t *set, int sig) {
    if (!sig_valid(sig)) return -1;
    *set |= sig_bit(sig);
    return 0;
}

static inline int
sigdelset(sigset_t *set, int sig) {
    if (!sig_valid(sig)) return -1;
    *set &= ~sig_bit(sig);
    return 0;
}

static inline int
sigismember(const sigset_t *set, int sig) {
    if (!sig_valid(sig)) return 0;
    return (*set & sig_bit(sig)) != 0;
}

struct Sigframe {
    struct Trapframe sf_tf;
    sigset_t sf_oldmask;
    siginfo_t sf_info;
    void *sf_handler;
    int sf_flags;
};

struct SigQueueEntry {
    int signo;
    sigval_t value;
};

#endif /* JOS_INC_SIGNAL_H */

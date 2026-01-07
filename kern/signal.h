#ifndef JOS_KERN_SIGNAL_H
#define JOS_KERN_SIGNAL_H

#include <inc/env.h>
#include <inc/signal.h>

void sig_init_env(struct Env *env);
int sigqueue_env(struct Env *env, int sig, sigval_t value);
void sig_deliver_pending(struct Env *env, struct Trapframe *tf);

#endif /* JOS_KERN_SIGNAL_H */

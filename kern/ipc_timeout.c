#include <inc/types.h>
#include <inc/error.h>
#include <inc/env.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/spinlock.h>

extern struct Env envs[NENV];

static inline uint64_t
now_ms(void) {
    extern int gettime(void);
    return (uint64_t)gettime() * 1000ULL;
}

void
ipc_timeout_tick(void) {
    uint64_t now = now_ms();
    for (size_t i = 0; i < NENV; i++) {
        struct Env *e = &envs[i];
        if (e->env_status == ENV_NOT_RUNNABLE) {
            /* Handle recv timeout */
            if (e->env_ipc_recving) {
                if (e->env_ipc_deadline_ms && now >= e->env_ipc_deadline_ms && e->env_ipc_from == 0) {
                    lock_kernel();
                    e->env_ipc_recving = false;
                    e->env_status = ENV_RUNNABLE;
                    e->env_tf.tf_regs.reg_rax = -E_TIMEDOUT;
                    unlock_kernel();
                }
            }
            /* Handle pending send */
            if (e->env_ipc_sending) {
                /* Check target availability */
                struct Env *target = NULL;
                if (envid2env(e->env_ipc_send_target, &target, false) < 0 || !target) {
                    lock_kernel();
                    e->env_ipc_sending = false;
                    e->env_status = ENV_RUNNABLE;
                    e->env_tf.tf_regs.reg_rax = -E_BAD_ENV;
                    unlock_kernel();
                    continue;
                }
                int sres = sys_ipc_try_send(e->env_ipc_send_target, e->env_ipc_send_value,
                                            e->env_ipc_send_srcva, e->env_ipc_send_size, e->env_ipc_send_perm);
                if (sres == 0) {
                    lock_kernel();
                    e->env_ipc_sending = false;
                    e->env_status = ENV_RUNNABLE;
                    e->env_tf.tf_regs.reg_rax = 0;
                    unlock_kernel();
                } else if (sres == -E_BAD_ENV) {
                    lock_kernel();
                    e->env_ipc_sending = false;
                    e->env_status = ENV_RUNNABLE;
                    e->env_tf.tf_regs.reg_rax = -E_BAD_ENV;
                    unlock_kernel();
                } else {
                    /* Not receving yet; check timeout */
                    if (e->env_ipc_send_deadline_ms && now >= e->env_ipc_send_deadline_ms) {
                        lock_kernel();
                        e->env_ipc_sending = false;
                        e->env_status = ENV_RUNNABLE;
                        e->env_tf.tf_regs.reg_rax = -E_TIMEDOUT;
                        unlock_kernel();
                    }
                }
            }
        }
    }
}

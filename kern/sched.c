#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/monitor.h>
#include <kern/kclock.h>


struct Taskstate cpu_ts;
_Noreturn void sched_halt(void);

uint64_t
now_ms(void) {
    return (uint64_t)gettime() * 1000ULL;
}
/* Choose a user environment to run and run it */
_Noreturn void
sched_yield(void) {
    /* Implement simple round-robin scheduling.
     *
     * Search through 'envs' for an ENV_RUNNABLE environment in
     * circular fashion starting just after the env was
     * last running.  Switch to the first such environment found.
     *
     * If no envs are runnable, but the environment previously
     * running is still ENV_RUNNING, it's okay to
     * choose that environment.
     *
     * If there are no runnable environments,
     * simply drop through to the code
     * below to halt the cpu */
     
	 /* Проверяем таймауты для всех процессов */
    for (int i = 0; i < NENV; i++) {
        if (envs[i].env_status == ENV_NOT_RUNNABLE && 
            envs[i].env_ipc_recving && 
            envs[i].env_ipc_timeout > 0) {
            
            /* Проверяем, не истек ли таймаут */
            uint64_t current_time = now_ms();
            uint64_t elapsed_time = current_time - envs[i].env_ipc_start;
            
            if (elapsed_time >= envs[i].env_ipc_timeout) {
                /* Таймаут истек, пробуждаем процесс с ошибкой */
                envs[i].env_ipc_timed_out = 1;
                envs[i].env_ipc_recving = 0;
                envs[i].env_status = ENV_RUNNABLE;
                
                /* Сбрасываем поля IPC */
                envs[i].env_ipc_from = 0;
                envs[i].env_ipc_value = 0;
                envs[i].env_ipc_perm = 0;
            }
        }
    }
    // LAB 3: Your code here:
    const size_t last_sched = curenv ? curenv - envs : 0;

    size_t next = (last_sched + 1) % NENV;

    next %= NENV;
    while (next != last_sched) {
        if (envs[next].env_status == ENV_RUNNABLE || envs[next].env_status == ENV_RUNNING) {
            break;
        }
        next++;
        next %= NENV;
    }

    if (envs[next].env_status == ENV_RUNNABLE || envs[next].env_status == ENV_RUNNING) {
        env_run(&envs[next]);
    }

    cprintf("Halt\n");

    /* No runnable environments,
     * so just halt the cpu */
    sched_halt();
}

/* Halt this CPU when there is nothing to do. Wait until the
 * timer interrupt wakes it up. This function never returns */
_Noreturn void
sched_halt(void) {

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor */
    int i;
    for (i = 0; i < NENV; i++)
        if (envs[i].env_status == ENV_RUNNABLE ||
            envs[i].env_status == ENV_RUNNING) break;
    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        for (;;) monitor(NULL);
    }

    /* Mark that no environment is running on CPU */
    curenv = NULL;

    /* Reset stack pointer, enable interrupts and then halt */
    asm volatile(
            "movq $0, %%rbp\n"
            "movq %0, %%rsp\n"
            "pushq $0\n"
            "pushq $0\n"
            "sti\n"
            "hlt\n" ::"a"(cpu_ts.ts_rsp0));

    /* Unreachable */
    for (;;)
        ;
}

/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/console.h>
#include <kern/env.h>
#include <kern/kclock.h>
#include <kern/pmap.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/trap.h>
#include <kern/traceopt.h>

static inline void
copy_from_user_asm(void *dst, const void *src, size_t n) {
    const uint8_t *s = (const uint8_t *)src;
    uint8_t *d = (uint8_t *)dst;

    for (size_t i = 0; i < n; i++) {
        asm volatile(
            "movb (%0), %%al\n\t"
            "movb %%al, (%1)\n\t"
            :
            : "r"(s + i), "r"(d + i)
            : "al", "memory"
        );
    }
}


/* Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors. */
static int
sys_cputs(const char *s, size_t len) {
    if (len > 0)
        user_mem_assert(curenv, s, len, PROT_R | PROT_USER_);

    char buf[256];

    while (len > 0) {
        size_t m = len;
        if (m > sizeof(buf)) m = sizeof(buf);

        copy_from_user_asm(buf, s, m);

        for (size_t i = 0; i < m; i++)
            cputchar(buf[i]);

        s += m;
        len -= m;
    }

    return 0;
}

/* Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting. */
static int
sys_cgetc(void) {
    // LAB 8: Your code here
    return cons_getc();
    return 0;
}

/* Returns the current environment's envid. */
static envid_t
sys_getenvid(void) {
    // LAB 8: Your code here
    assert(curenv);
    return curenv->env_id;
    return 0;
}

/* Destroy a given environment (possibly the currently running environment).
 *
 *  Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid. */
static int
sys_env_destroy(envid_t envid) {
    // LAB 8: Your code here.
    struct Env *env;
    int r = envid2env(envid, &env, true);
    if (r < 0)
        return r;

    if (trace_envs) {
        cprintf(env == curenv ?
                        "[%08x] exiting gracefully\n" :
                        "[%08x] destroying %08x\n",
                curenv->env_id, env->env_id);
    }

    env_destroy(env);

    return 0;
}

/* Dispatches to the correct kernel function, passing the arguments. */
uintptr_t
syscall(uintptr_t syscallno, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
    /* Call the function corresponding to the 'syscallno' parameter.
     * Return any appropriate return value. */

    // LAB 8: Your code here

    switch (syscallno) {
    case SYS_cputs:
        sys_cputs((const char *)a1, (size_t)a2);
        return 0;
    case SYS_cgetc:
        return sys_cgetc();
    case SYS_getenvid:
        return sys_getenvid();
    case SYS_env_destroy:
        return sys_env_destroy((envid_t)a1);
    default:
        return -E_NO_SYS;
    }

    return -E_NO_SYS;
}

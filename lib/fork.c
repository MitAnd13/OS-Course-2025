/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>

/* User-level fork with copy-on-write.
 * Create a child.
 * Lazily copy our address space and page fault handler setup to the child.
 * Then mark the child as runnable and return.
 *
 * Returns: child's envid to the parent, 0 to the child, < 0 on error.
 * It is also OK to panic on error.
 *
 * Hint:
 *   Use sys_map_region, it can perform address space copying in one call
 *   Don't forget to set page fault handler in the child (using sys_env_set_pgfault_upcall()).
 *   Remember to fix "thisenv" in the child process.
 */

envid_t
fork(void) {
    // LAB 9: Your code here.
    envid_t child = sys_exofork();
    if (child < 0)
        panic("sys_exofork: %d", child);

    if (child == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    void *uxstack_bottom =
        (void *)(uintptr_t)(USER_EXCEPTION_STACK_TOP - USER_EXCEPTION_STACK_SIZE);

    int r = sys_map_region(0, (void *)0,
                           child, (void *)0,
                           (size_t)(uintptr_t)uxstack_bottom,
                           PROT_ALL | PROT_LAZY | PROT_COMBINE);
    if (r < 0)
        panic("sys_map_region: %d", r);

    r = sys_alloc_region(child, uxstack_bottom,
                         USER_EXCEPTION_STACK_SIZE,
                         PROT_R | PROT_W | ALLOC_ZERO);
    if (r < 0)
        panic("sys_alloc_region (uxstack): %d", r);

    if (thisenv->env_pgfault_upcall) {
        r = sys_env_set_pgfault_upcall(child, thisenv->env_pgfault_upcall);
        if (r < 0)
            panic("sys_env_set_pgfault_upcall: %d", r);
    }

    r = sys_env_set_status(child, ENV_RUNNABLE);
    if (r < 0)
        panic("sys_env_set_status: %d", r);

    return child;
}

envid_t
sfork() {
    panic("sfork() is not implemented");
}
#include <inc/lib.h>

void
umain(int argc, char **argv) {
    USED(argc); USED(argv);
    envid_t child = sys_exofork();
    if (child < 0) {
        cprintf("exofork failed: %i\n", child);
        return;
    }
    if (child == 0) {
        sys_yield();
        sys_yield();
        exit();
    }
    int r = ipc_send_timeout(child, 42, NULL, 0, 0, 500);
    if (r == -E_TIMEDOUT)
        cprintf("send_timeout: OK (-E_TIMEDOUT)\n");
    else
        cprintf("send_timeout: FAIL (%i)\n", r);
    r = ipc_send_timeout(0xDEADBEEF, 7, NULL, 0, 0, 100);
    if (r == -E_BAD_ENV)
        cprintf("send_bad_env: OK (-E_BAD_ENV)\n");
    else
        cprintf("send_bad_env: FAIL (%i)\n", r);
}


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
    ipc_send_timeout(child, 42, NULL, 0, 0, 5);

}


#include <inc/lib.h>

static void
receiver(void) {
    envid_t from = 0;
    size_t sz = PAGE_SIZE;
    int perm = 0;
    int32_t v = ipc_recv_timeout(&from, NULL, &sz, &perm, 5000);
    if (v >= 0)
        cprintf("recv_normal: OK (val=%d from=%08x)\n", v, from);
    else
        cprintf("recv_normal: FAIL (%i)\n", v);
}

void
umain(int argc, char **argv) {
    USED(argc); USED(argv);
    envid_t child = sys_exofork();
    if (child < 0) {
        cprintf("exofork failed: %i\n", child);
        return;
    }
    if (child == 0) {
        receiver();
        exit();
    }
    ipc_send_timeout(child, 123, NULL, 0, 0, 2000);
    cprintf("send_normal: OK\n");

}


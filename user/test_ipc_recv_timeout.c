#include <inc/lib.h>

void
umain(int argc, char **argv) {
    USED(argc); USED(argv);
    envid_t from = 0;
    size_t sz = PAGE_SIZE;
    int perm = 0;
    int32_t v = ipc_recv_timeout(&from, NULL, &sz, &perm, 10);
    if (v == -E_IPC_TIMEOUT)
        cprintf("recv_timeout: OK (-E_IPC_TIMEOUT)\n");
    else
        cprintf("recv_timeout: FAIL (%i)\n", v);
}


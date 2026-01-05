#include <inc/syscall.h>
#include <inc/futex.h>

int futex_wait(int *uaddr, int val) {
    int cur = __sync_fetch_and_add(uaddr, 0);
    if (cur != val) return -1;
    return syscall(SYS_FUTEX_WAIT, uaddr, val, 0, 0, 0, 0);
}

// Similar for wake and set_robust_list

int futex_wake(int *uaddr, int val) {
    int cur = __sync_fetch_and_add(uaddr, 0);
    if (cur != val) return -1;
    return syscall(SYS_FUTEX_WAKE, uaddr, val, 0, 0, 0, 0);
}

int set_robust_list(int *uaddr, int val) {
    int cur = __sync_fetch_and_add(uaddr, 0);
    if (cur != val) return -1;
    return syscall(SYS_SET_ROBUST_LIST, uaddr, val, 0, 0, 0, 0);
}
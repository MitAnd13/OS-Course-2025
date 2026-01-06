#ifndef INC_FUTEX_H
#define INC_FUTEX_H

#include <inc/types.h>
#include <inc/queue.h>  // Your provided BSD queue.h
#include <kern/spinlock.h>  
#include <inc/memlayout.h>

struct FutexWaiter {
    LIST_ENTRY(FutexWaiter) link;
    struct Env *env;
    uintptr_t uaddr;
    int val;
};

struct FutexBucket {
    struct spinlock lock;  // From spinlock.h
    LIST_HEAD(FutexWaiterList, FutexWaiter) waiters;
};

// Robust structures (user-space)
struct robust_list {
    struct robust_list *next;
    uintptr_t futex_addr;
};
struct robust_list_head {
    struct robust_list list;
    long futex_offset;
};

#endif

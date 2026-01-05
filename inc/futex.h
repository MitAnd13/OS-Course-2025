#ifndef INC_FUTEX_H
#define INC_FUTEX_H

#include <inc/types.h>
#include <inc/queue.h>  // Your provided BSD queue.h
#include <kern/spinlock.h>  
#include <inc/memlayout.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

struct FutexWaiter {
    LIST_ENTRY(FutexWaiter) link;
    struct Env *env;
    uintptr_t uaddr;
    int val;
};

#define FUTEX_HASHSIZE 256  // Fixed, tunable
struct FutexBucket {
    struct spinlock lock;  // From spinlock.h
    LIST_HEAD(FutexWaiterList, FutexWaiter) waiters;
};
extern struct FutexBucket futex_buckets[FUTEX_HASHSIZE];

// Robust structures (user-space)
struct robust_list {
    struct robust_list *next;
    uintptr_t futex_addr;
};
struct robust_list_head {
    struct robust_list list;
    long futex_offset;
};

void futex_init(void);
int sys_futex_wait(uintptr_t uaddr, int val);
int sys_futex_wake(uintptr_t uaddr, int num);
int sys_set_robust_list(uintptr_t head, size_t len);
void futex_handle_death(struct Env *env);

#endif
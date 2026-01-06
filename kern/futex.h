#ifndef KERN_FUTEX_H
#define KERN_FUTEX_H

#include <inc/futex.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define FUTEX_HASHSIZE 256  // Fixed, tunable

extern struct FutexBucket futex_buckets[FUTEX_HASHSIZE];

void futex_init(void);
int sys_futex_wait(uintptr_t uaddr, int val);
int sys_futex_wake(uintptr_t uaddr, int num);
int sys_set_robust_list(uintptr_t head, size_t len);
void futex_handle_death(struct Env *env);

#endif

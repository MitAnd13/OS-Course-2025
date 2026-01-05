#include <inc/futex.h>
#include <inc/memlayout.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/env.h>
#include <inc/trap.h>
#include <inc/syscall.h>
#include <kern/alloc.h>
#include <kern/pmap.h>
#include <kern/spinlock.h>
#include <kern/sched.h>
#include <kern/env.h>  // Для env_block/unblock

struct FutexBucket futex_buckets[FUTEX_HASHSIZE];

void futex_init(void) {
    for (int i = 0; i < FUTEX_HASHSIZE; i++) {
        spin_initlock(&futex_buckets[i].lock);
        LIST_INIT(&futex_buckets[i].waiters);
    }
}

static uint32_t futex_hash(uintptr_t uaddr) {
    return (uaddr >> 3) % FUTEX_HASHSIZE;
}

static int futex_validate_addr(struct Env *env, uintptr_t uaddr, pte_t **pte_store) {
    if (uaddr >= MAX_USER_ADDRESS) return -E_INVAL;
    
    // В вашей системе может быть env->env_pgdir или env->env_cr3
    // Уточните структуру Env в вашей кодовой базе
    pde_t *pgdir = env->address_space.pml4;
    if (!pgdir) return -E_INVAL;
    
    return page_lookup(pgdir, (void*)ROUNDDOWN(uaddr, HUGE_PAGE_SIZE), pte_store);
}

static int futex_get_user_val(struct Env *env, uintptr_t uaddr, int *val) {
    pte_t *pte;
    int r = futex_validate_addr(env, uaddr, &pte);
    if (r < 0) return r;
    
    if (!(*pte & PTE_U) || !(*pte & PTE_P)) 
        return -E_FAULT;
    
    // Безопасное чтение из пользовательской памяти
    // Вам нужна функция типа user_mem_check
    if (user_mem_check(env, (void*)uaddr, sizeof(int), PTE_U|PTE_P) < 0)
        return -E_FAULT;
    
    *val = *(int*)uaddr;
    return 0;
}

int sys_futex_wait(uintptr_t uaddr, int val) {
    struct Env *cur = curenv;
    int cur_val;
    
    int r = futex_get_user_val(cur, uaddr, &cur_val);
    if (r < 0) return r;
    
    if (cur_val != val) 
        return -E_AGAIN;  // Используйте существующую константу ошибки
    
    uint32_t hash = futex_hash(uaddr);
    struct FutexBucket *bucket = &futex_buckets[hash];
    
    struct FutexWaiter *waiter = kmalloc(sizeof(*waiter));
    if (!waiter) return -E_NO_MEM;
    
    waiter->env = cur;
    waiter->uaddr = uaddr;
    waiter->val = val;
    
    spin_lock(&bucket->lock);
    LIST_INSERT_HEAD(&bucket->waiters, waiter, link);
    spin_unlock(&bucket->lock);
    
    // Блокируем текущий env
    cur->env_status = ENV_NOT_RUNNABLE;
    sched_yield();
    
    // После пробуждения - waiter должен быть удален из списка
    // в sys_futex_wake, так что kmalloc здесь не нужен
    return 0;
}

int sys_futex_wake(uintptr_t uaddr, int num) {
    uint32_t hash = futex_hash(uaddr);
    struct FutexBucket *bucket = &futex_buckets[hash];
    int woken = 0;
    
    spin_lock(&bucket->lock);
    
    struct FutexWaiter *w, *tmp;
    // Исправленный цикл - без LIST_FOREACH_SAFE
    w = LIST_FIRST(&bucket->waiters);
    while (w != NULL) {
        tmp = LIST_NEXT(w, link);
        if (w->uaddr == uaddr) {
            LIST_REMOVE(w, link);
            w->env->env_status = ENV_RUNNABLE;
            kfree(w);
            woken++;
            if (woken >= num) break;
        }
        w = tmp;
    }
    
    spin_unlock(&bucket->lock);
    return woken;
}


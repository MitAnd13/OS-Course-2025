/* User-level IPC library routines */

#include <inc/lib.h>

/* Receive a value via IPC and return it.
 * If 'pg' is nonnull, then any page sent by the sender will be mapped at
 *    that address.
 * If 'from_env_store' is nonnull, then store the IPC sender's envid in
 *    *from_env_store.
 * If 'perm_store' is nonnull, then store the IPC sender's page permission
 *    in *perm_store (this is nonzero iff a page was successfully
 *    transferred to 'pg').
 * If the system call fails, then store 0 in *fromenv and *perm (if
 *    they're nonnull) and return the error.
 * Otherwise, return the value sent by the sender
 *
 * Hint:
 *   Use 'thisenv' to discover the value and who sent it.
 *   If 'pg' is null, pass sys_ipc_recv a value that it will understand
 *   as meaning "no page".  (Zero is not the right value, since that's
 *   a perfectly valid place to map a page.) */
int32_t
ipc_recv(envid_t *from_env_store, void *pg, size_t *size, int *perm_store) {
    // LAB 9: Your code here:
    if (!pg) {
        pg = (void *)MAX_USER_ADDRESS;
    }

    int res = sys_ipc_recv(pg, PAGE_SIZE);

    if (res) {
        if (from_env_store) {
            *from_env_store = 0;
        }

        if (perm_store) {
            *perm_store = 0;
        }

        return res;
    } else {
        if (from_env_store) {
            *from_env_store = thisenv->env_ipc_from;
        }

        if (perm_store && pg != (void *)MAX_USER_ADDRESS) {
            *perm_store = thisenv->env_ipc_perm;
        }

        if (size) {
            *size = PAGE_SIZE;
        }

        return thisenv->env_ipc_value;
    }
}

/* Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
 * This function keeps trying until it succeeds.
 * It should panic() on any error other than -E_IPC_NOT_RECV.
 *
 * Hint:
 *   Use sys_yield() to be CPU-friendly.
 *   If 'pg' is null, pass sys_ipc_recv a value that it will understand
 *   as meaning "no page".  (Zero is not the right value.) */
void
ipc_send(envid_t to_env, uint32_t val, void *pg, size_t size, int perm) {
    // LAB 9: Your code here:
    if (!pg) {
        pg = (void *)MAX_USER_ADDRESS;
    }
    
    int res;

    do {
        res = sys_ipc_try_send(to_env, (uint64_t)val, pg, size, perm);
        
        if (res && res != -E_IPC_NOT_RECV) {
            panic("ipc_send: failed to send value %u to env %d, errno is %i\n", val, to_env, res);
        }

        sys_yield();
    } while (res); 
}

/* Find the first environment of the given type.  We'll use this to
 * find special environments.
 * Returns 0 if no such environment exists. */
envid_t
ipc_find_env(enum EnvType type) {
    for (size_t i = 0; i < NENV; i++)
        if (envs[i].env_type == type)
            return envs[i].env_id;
    return 0;
}

void
ipc_send_timeout(envid_t to_env, uint32_t val, void *pg, 
                 size_t size, int perm, uint64_t timeout_s) {
     void *srcva = pg ? pg : (void *)MAX_USER_ADDRESS;
    size_t sendsz = pg ? size : 0;
    int sendperm = pg ? perm : 0;
    uint64_t start_ms = (uint64_t)sys_gettime()*1000LLU;
    uint64_t deadline = start_ms + timeout_s*1000LLU;

    for (;;) {
		//cprintf("T!\n");
        int r = sys_ipc_try_send(to_env, (uint64_t)val, srcva, sendsz, sendperm);
        //cprintf("E!\n");
        if (r == 0)
            return;
        if (sys_gettime()*1000LLU >= deadline) {
			cprintf("Timeout!\n");
			break;
		}
        if (r == -E_IPC_NOT_RECV) {
            sys_yield();
            continue;
        }
        panic("ipc_send: %i", r);
    }
}

int32_t
ipc_recv_timeout(envid_t *from_env_store, void *pg, size_t *size, 
                 int *perm_store, uint64_t timeout_s) {
    void *dstva = pg ? pg : (void *)MAX_USER_ADDRESS;
    size_t maxsz = pg ? (size ? *size : PAGE_SIZE) : 0;
    
    uint64_t timeout_ms = timeout_s * 1000ULL;
    int r = sys_ipc_recv_timeout(dstva, maxsz, timeout_ms);
    if (thisenv->env_ipc_timed_out != 0 && r == 0)
		//cprintf("%d", r);
		r = -E_IPC_TIMEOUT;
    if (r < 0) {
        if (r == -E_IPC_TIMEOUT) {
            /* Возвращаем ошибку таймаута */
            return -E_IPC_TIMEOUT;
        }
        if (r == -E_BAD_TIMEOUT) {
            return -E_BAD_TIMEOUT;
        }
        if (from_env_store) *from_env_store = 0;
        if (perm_store) *perm_store = 0;
        if (size) *size = 0;
        return r;
    }
    
    if (from_env_store) *from_env_store = thisenv->env_ipc_from;
    if (perm_store) *perm_store = thisenv->env_ipc_perm;
    if (size) *size = thisenv->env_ipc_maxsz;
    
    return (int32_t)thisenv->env_ipc_value;
}

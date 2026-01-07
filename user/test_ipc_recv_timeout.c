#include <inc/lib.h>

void
umain(int argc, char **argv) {
    USED(argc); USED(argv);
    cprintf("recv_timeout: starting\n");
    envid_t from = 0;
    size_t sz = PAGE_SIZE;
    int perm = 0;
    int32_t v = ipc_recv_timeout(&from, NULL, &sz, &perm, 3);
    if (v == -E_IPC_TIMEOUT)
        cprintf("recv_timeout: OK (-E_IPC_TIMEOUT)\n");
    else
        cprintf("recv_timeout: FAIL (%i)\n", v);
    
    
        
    envid_t child = fork();
    if (child < 0) {
        cprintf("fork failed: %i\n", child);
        return;
    }
    if (child == 0) {
		int32_t v = ipc_recv_timeout(&from, NULL, &sz, &perm, 3);
		if (v == -E_IPC_RECV_WAIT)
			cprintf("recv_wait_drop: OK (-E_IPC_RECV_WAIT)\n");
		else
			cprintf(" FAIL (%i)\n", v);
		exit();
    }
    for (int i=0; i <= 100000;i++)
    {
		v = sys_env_set_status(child, ENV_RUNNABLE);
		//cprintf("%d %d\n", v, i);
		
	}
    //cprintf("%d\n", v);
    wait(child);
     
}


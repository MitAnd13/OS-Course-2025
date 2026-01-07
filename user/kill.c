#include <inc/lib.h>

static void
usage(void) {
    cprintf("usage: kill -SIGNAL ENVID\n");
    exit();
}

void
umain(int argc, char **argv) {
    if (argc != 3)
        usage();

    char *end = NULL;
    long sig = strtol(argv[1], &end, 0);
    if (end == argv[1] || *end)
        usage();
        
    if (sig < 0)
        sig = -sig;

    end = NULL;
    long envid = strtol(argv[2], &end, 0);
    if (end == argv[2] || *end)
        usage();

    int res = sigkill((envid_t)envid, (int)sig);
    if (res < 0)
        cprintf("kill: %i\n", res);
}

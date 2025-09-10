#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct spinlock wait_lock;

int myfunction(char *str) {
    printf("%s\n", str);
    return 0xABCDABCD;
}

// Wrapper for my_syscall
int sys_myfunction(void) {
    char str[100];
    if(argstr(0, str, 100) < 0)
        return -1;
    return myfunction(str);
}

uint64 sys_getppid(void) {
    int ppid = 0;
    struct proc *p = myproc();

    acquire(&wait_lock);
    if(p->parent) ppid = p->parent->pid;
    release(&wait_lock);

    return ppid;
}
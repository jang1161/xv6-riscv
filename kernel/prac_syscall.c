#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc_queue.h"

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

uint64 sys_getlev(void) {
    return (int)sched_type == (int)MLFQ ? (uint64)myproc()->level : 99;
}

int setpriority(int pid, int priority) {
    if(priority < 0 || priority > 3)
        return -2;

    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->pid == pid) {
            acquire(&p->lock);
            p->priority = priority;
            release(&p->lock);
            return 0;
        }
    }
    return -1;
}

uint64 sys_setpriority(void) {
    int pid, priority;
    argint(0, &pid);
    argint(1, &priority);
    return setpriority(pid, priority);
}

uint64 sys_mlfqmode(void) {
    if((int)sched_type == (int)MLFQ)
        return -1;

    intr_off();
    for(int i = 0; i < MLFQLV; i++)
        q_init(&mlfq[i]);
    
    int pid = 0;
    acquire(&mlfq[0].lock);
    while(pid < NPROC) {
        struct proc *p;
        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->pid == pid && p->state != UNUSED && p->state != ZOMBIE) {
                p->priority = 3;
                p->remain_time = 1;
                p->level = 0;
                q_add(&mlfq[0], p);
            }
            release(&p->lock);
        }
        pid++;
    }

    release(&mlfq[0].lock);
    sched_type = MLFQ;
    intr_on();
    return 0;
}

uint64 sys_fcfsmode(void) {
    if((int)sched_type == (int)FCFS)
        return -1;

    intr_off();
    crtpid = -1;
    called_yield = 0;

    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        p->priority = -1;
        p->level = -1;
        p->remain_time = -1;
        release(&p->lock);
    }

    sched_type = FCFS;
    intr_on();
    return 0;
}
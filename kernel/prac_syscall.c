#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc_queue.h"

extern struct spinlock wait_lock;
extern void forkret(void);

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

/*
similar with fork
fork() calls allocproc(), clone() calls allocproc_thread()
set trapframe manually 
*/
int clone(void(*fnc)(void*, void*), void *arg1, void *arg2, void *stack) {
	int i, pid;
	struct proc *np;
	struct proc *p = myproc();

	if((np = allocproc_thread(p)) == 0) {
		return -1;
	}

	np->sz = p->sz;

	np->trapframe->epc = (uint64)fnc;
	np->trapframe->a0 = (uint64)arg1;
	np->trapframe->a1 = (uint64)arg2;
	np->trapframe->sp = (uint64)stack + PGSIZE;

	np->ustack = (void *)(uint64)stack;

	for(i = 0; i < NOFILE; i++)
		if(p->ofile[i])
			np->ofile[i] = filedup(p->ofile[i]);
	np->cwd = idup(p->cwd);

	safestrcpy(np->name, p->name, sizeof(p->name));

	pid = np->pid;

	release(&np->lock);

	acquire(&wait_lock);
	np->parent = p;
	release(&wait_lock);

	acquire(&np->lock);
	np->state = RUNNABLE;
	release(&np->lock);

	return pid;
}

uint64 sys_clone(void) {
	uint64 fnc, stack;
	int arg1, arg2;
	argaddr(0, &fnc);
	argint(1, &arg1);
	argint(2, &arg2);
	argaddr(3, &stack);

	return clone((void*)fnc, (void*)(uint64)arg1, (void*)(uint64)arg2, (void*)stack);
}

/*
p->isMain set to 0, because a thread created by clone is NOT a main thread
p->main is caller's main
pagetable is not newly allocated, use same with main thread's and freeproc()
has to be modified not to deallocate pagetable when a thread not main is being freed
*/
struct proc* allocproc_thread(struct proc *parent) {
	struct proc *p;

	for(p = proc; p < &proc[NPROC]; p++) {
		acquire(&p->lock);
		if(p->state == UNUSED) {
		goto found;
		} else {
		release(&p->lock);
		}
	}
	return 0;

found:
	p->isMain = 0;
	p->main = parent->main;
	p->tid = p->main->nexttid++;

	p->pid = allocpid();
	p->state = USED;

	uint64 main_tf_base = TRAPFRAME - PGSIZE;
	uint64 new_tf_base = main_tf_base - p->tid * PGSIZE;
	void *pa;
	if((pa = kalloc()) == 0) {
		freeproc(p);
		release(&p->lock);
		return 0;
	}
	
	if(mappages(parent->main->pagetable, new_tf_base, PGSIZE, (uint64)pa, PTE_R | PTE_W) < 0) {
		kfree(pa);
    freeproc(p);
    release(&p->lock);
    return 0;
  }
	p->trapframe = (struct trapframe*)pa;
	p->tf_va = new_tf_base;
	p->pagetable = p->main->pagetable;

	memset(&p->context, 0, sizeof(p->context));
	p->context.ra = (uint64)forkret;
	p->context.sp = p->kstack + PGSIZE;

	return p;
}

int join(void **stack) {
	struct proc *pp;
	int havekids, pid;
	struct proc *p = myproc();

	acquire(&wait_lock);

	for(;;){
		// Scan through table looking for exited children.
		havekids = 0;
		for(pp = proc; pp < &proc[NPROC]; pp++){
			if(pp->parent == p){
				// make sure the child isn't still in exit() or swtch().
				acquire(&pp->lock);

				havekids = 1;
				if(pp->state == ZOMBIE){
				// Found one.
				pid = pp->pid;
				
				if(copyout(p->pagetable, (uint64)stack, (char *)&pp->ustack, sizeof(pp->ustack)) < 0) {
					release(&pp->lock);
					release(&wait_lock);
					return -1;
				}

				freeproc(pp);
				release(&pp->lock);
				release(&wait_lock);
				return pid;
				}
				release(&pp->lock);
			}
		}

		// No point waiting if we don't have any children.
		if(!havekids || killed(p)){
		release(&wait_lock);
		return -1;
		}
		
		// Wait for a child to exit.
		sleep(p, &wait_lock);  //DOC: wait-sleep
	}
}

uint64 sys_join(void) {
	uint64 stack;
	argaddr(0, &stack);
	return join((void **)stack);
}
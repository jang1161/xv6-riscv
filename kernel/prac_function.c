#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

/*
for exec(), p may not a main thread
does not free pagetable but trapframe has to be unmapped
and p's trapframe should be mapped at TRAPFRAME
similar with freeproc()
*/
int cleanUpAllThreads(struct proc *p) {
	for(struct proc *o = proc; o < &proc[NPROC]; o++) {
		if(o == p) continue;

		acquire(&o->lock);
		if(o->main == p->main) {
			uvmunmap(o->pagetable, (uint64)o->tf_va, 1, 1);

			o->trapframe = 0;
			o->tf_va = 0;

			o->sz = 0;
			o->pid = 0;
			o->parent = 0;
			o->name[0] = 0;
			o->chan = 0;
			o->killed = 0;
			o->xstate = 0;
			o->state = UNUSED;
		}
		release(&o->lock);
	}

	acquire(&p->lock);
	uvmunmap(p->pagetable, (uint64)p->tf_va, 1, 0);
	
	if(mappages(p->pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(p->pagetable, TRAMPOLINE, 1, 0);
    uvmfree(p->pagetable, 0);
    return -1;
  }

	p->isMain = 1;
	p->tid = 0;
	p->nexttid = 1;
	p->main = p;
	p->tf_va = TRAPFRAME;

	release(&p->lock);
	return 0;
}
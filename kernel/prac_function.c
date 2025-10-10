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
	struct proc *parent = 0;
	if(p->tid == 0)
		parent = p->parent;

	for(struct proc *o = proc; o < &proc[NPROC]; o++) {
		if(o == p) continue;

		acquire(&o->lock);
		if(o->main == p->main) {
			if(o->tid == 0)
				parent = o->parent;

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
	p->parent = parent;

	release(&p->lock);
	return 0;
}

int cow_handler(struct proc *p, uint64 va, pte_t *pte, int reset_epc) {
	uint64 pa = PTE2PA(*pte);
	uint flags;
	char *mem;

	acquire(&reflock);
	if(ref_count[pa / PGSIZE] > 1) {
		ref_count[pa / PGSIZE]--;
		release(&reflock);

		if((mem = kalloc()) == 0) {
			return -1;
		}
		memmove(mem, (char*)pa, PGSIZE);
		
		flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
	
		uvmunmap(p->pagetable, va, 1, 0);
		if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, flags) != 0) {
			kfree(mem);
			return -1;
		}
	} 
	else { // only one is referencing this pte
		*pte &= ~PTE_COW;
		*pte |= PTE_W;
		release(&reflock);
	}
	
	// if(reset_epc)
	// 	p->trapframe->epc = r_sepc();

	return 0;
}
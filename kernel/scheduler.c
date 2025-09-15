#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc_queue.h"

void schedule_fcfs(struct cpu *c, struct proc proc[], int nextpid) {
    struct proc *p;
    int found = 0;

    int minpid = nextpid;
      struct proc *selected = 0, *prev = 0;

      for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == RUNNABLE) {
          found = 1;
          if(p->pid == crtpid) { // process has been excuted
            if(called_yield) { // called yield, so search other processes (and keep lock)
              prev = p;
            } else { // resume
              selected = p;
              break;
            }
          }
          else if(p->pid < minpid) { // higher priority
            if(selected != 0) // release previous selected's lock
              release(&selected->lock);
            selected = p;
            minpid = p->pid;
          }
          else { 
            release(&p->lock); // lower priority, release lock
          }
        } else { // not runnable
          release(&p->lock);
        }
      }

      if(found == 0) {
        asm volatile("wfi");
      } else {
        if(selected == 0) // only one process is RUNNABLE, which called yield 
          selected = prev;
        
        // has lock
        selected->state = RUNNING;
        c->proc = selected;
        swtch(&c->context, &selected->context);
        c->proc = 0;
        release(&selected->lock);

        crtpid = selected->pid;
        called_yield = 0;

        if(prev && selected != prev) // prev not used
          release(&prev->lock);
      }
}

// void schedule_mlfq(struct cpu *c, struct pqueue mlfq[], )
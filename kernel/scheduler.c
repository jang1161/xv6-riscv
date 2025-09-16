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

void schedule_mlfq(struct cpu *c, struct pqueue mlfq[]) {
  struct proc *p;
  int found = 0;
  int sz;

  // priority boosting
  if(ticks % 50  == 0) {
    acquire(&mlfq[0].lock);
    for(int i = 1; i < MLFQLV; i++) {
      acquire(&mlfq[i].lock);
      sz = mlfq[i].size;
      for(int j = 0; j < sz; j++) {
        q_add(&mlfq[0], q_poll(&mlfq[i]));
      }
      release(&mlfq[i].lock);
    }

    sz = mlfq[0].size;
    for(int i = 0; i < sz; i++) {
      struct proc * p = mlfq[0].elements[i];
      acquire(&p->lock);
      p->level = 0;
      p->remain_time = 1;
      release(&p->lock);
    }

    release(&mlfq[0].lock);
  }

  struct proc *selected = 0;

  for(int lv = 0; lv < MLFQLV; lv++) {
    acquire(&mlfq[lv].lock);

    if(lv < 2) {
      sz = mlfq[lv].size;
      for(int i = 0; i < sz; i++) {
        p = q_poll(&mlfq[lv]);
        acquire(&p->lock);

        if(p->state == RUNNABLE) {
          found = 1;
          selected = p;
          break; // keep lock
        } else {
          q_add(&mlfq[lv], p);
          release(&p->lock);
        }
      }
    } 

    else { // lv2
      sz = mlfq[lv].size;
      for(int i = 0; i < sz; i++) {
        p = q_poll(&mlfq[lv]);
        acquire(&p->lock);
        
        if(p->state == RUNNABLE) {
          if(!found) {
            found = 1;
            selected = p; // keep lock
          } 
          else if(p->priority > selected->priority) {
            q_add(&mlfq[lv], selected); // change selected
            release(&selected->lock);
            selected = p;
          } else {
            q_add(&mlfq[lv], p);
            release(&p->lock);
          }
        } 
        else { // not runnable
          q_add(&mlfq[lv], p);
          release(&p->lock);
        }
      }
    }

    release(&mlfq[lv].lock);
    if(found) break; // for found in lv0,1
  }

  if(found == 0) {
    asm volatile("wfi");
  } else {
    // has lock
    selected->state = RUNNING;
    c->proc = selected;
    swtch(&c->context, &selected->context);
    c->proc = 0;
    release(&selected->lock);

    // check state after come back
    acquire(&selected->lock);
    if(selected->state == ZOMBIE) {
      // terminated: do nothing
    } else {
      if(--selected->remain_time == 0) {
        if(selected->level < 2) {
          selected->level++;
        } else { // already in lv2
          selected->priority = selected->priority > 0 ? selected->priority - 1 : 0;
        }
        selected->remain_time = 2 * selected->level + 1;
      }

      acquire(&mlfq[selected->level].lock);
      q_add(&mlfq[selected->level], selected);
      release(&mlfq[selected->level].lock);
    }
    release(&selected->lock);
  }
}
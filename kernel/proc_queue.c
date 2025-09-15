#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc_queue.h"

void q_init(struct pqueue *q) {
	q->size = 0;
	q->front = 0;
	q->rear = -1;
}

int q_add(struct pqueue *q, struct proc *p) {
	if(q_full(q)) return -1;

	q->rear = (q->rear + 1) % NPROC;
	q->elements[q->rear] = p;
	q->size++;
	return 0;
}

struct proc *q_poll(struct pqueue *q) {
	if(q_empty(q)) return 0;

	struct proc *p = q->elements[q->front];
	q->front = (q->front + 1) % NPROC;
	q->size--;
	return p;
}

int q_empty(struct pqueue *q) {
	return q->size == 0;
}

int q_full(struct pqueue *q) {
	return q->size == NPROC;
}
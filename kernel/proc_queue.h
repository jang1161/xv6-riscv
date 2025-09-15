#include "proc.h"
#include "param.h"
#include "spinlock.h"

struct pqueue
{
	struct spinlock lock;
	struct proc *elements[NPROC];
	int size;
	int front;
	int rear;
};

void q_init(struct pqueue *q);
int q_add(struct pqueue *q, struct proc *p); // success: 0, fail: -1
struct proc *q_poll(struct pqueue *q); // success: *proc, fail: 0
int q_empty(struct pqueue *q);
int q_full(struct pqueue *q);

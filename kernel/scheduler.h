#include "proc.h"
#include "proc_queue.h"

void schedule_fcfs(struct cpu *c, struct proc proc[], int nextpid);
void schedule_mlfq(struct cpu *c, struct pqueue mlfq[]);
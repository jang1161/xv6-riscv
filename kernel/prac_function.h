#include "proc.h"

int cleanUpAllThreads(struct proc *p);
int cow_handler(struct proc *p, uint64 va, pte_t *pte);
#include "user/user.h"
#include "user/thread.h"
#include "kernel/types.h"
#include "kernel/riscv.h"

int thread_create(void(*start_routine)(void*, void*), void *arg1, void *arg2) {
    void *stack = malloc(2*PGSIZE);
    if(stack == 0) return -1;
    
    stack = (void *)PGROUNDUP((uint64)stack);
    memset(stack, 0, PGSIZE);
    
    return clone(start_routine, arg1, arg2, stack);
}

int thread_join(void) {
    void *stack;
    int pid = join(&stack);
    free(stack);
    return pid;
}
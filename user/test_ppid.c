#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("My student ID is 2021082215\n");
    printf("My pid is %d\n", getpid());
    printf("My ppid is %d\n", getppid());
    
    exit(0);
}
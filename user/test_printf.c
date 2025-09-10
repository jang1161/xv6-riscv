#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int pid = getpid();
    int ticks = uptime(), t = -1;
    
    int i = 100000;
    while(i--) {
        if(ticks != t) {
            printf("ticks = %d, pid = %d\n", ticks, pid);
            t = ticks;
        }
        ticks = uptime();
    }

    exit(0);
}
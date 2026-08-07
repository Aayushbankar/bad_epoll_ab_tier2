#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

struct msg_struct {
    long mtype;
    char mtext[144];
};

int get_kmalloc_192_count() {
    FILE *f = fopen("/proc/slabinfo", "r");
    if (!f) return -1;
    char line[256];
    int count = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "kmalloc-192", 11) == 0) {
            sscanf(line, "%*s %d", &count);
            break;
        }
    }
    fclose(f);
    return count;
}

int main() {
    printf("[*] Starting Cache Verification...\n");
    
    int before_epoll = get_kmalloc_192_count();
    printf("[*] kmalloc-192 before epoll_create1: %d\n", before_epoll);
    
    int epfds[100];
    for (int i=0; i<100; i++) {
        epfds[i] = epoll_create1(0);
    }
    
    int after_epoll = get_kmalloc_192_count();
    printf("[*] kmalloc-192 after 100 epolls: %d (diff: %d)\n", after_epoll, after_epoll - before_epoll);
    
    for (int i=0; i<100; i++) {
        close(epfds[i]);
    }
    
    int msgq = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    
    int before_msg = get_kmalloc_192_count();
    printf("[*] kmalloc-192 before msgsnd: %d\n", before_msg);
    
    struct msg_struct msg;
    msg.mtype = 1;
    memset(msg.mtext, 0x41, 144);
    
    for (int i=0; i<100; i++) {
        msgsnd(msgq, &msg, 144, IPC_NOWAIT);
    }
    
    int after_msg = get_kmalloc_192_count();
    printf("[*] kmalloc-192 after 100 msgsnd (144 bytes): %d (diff: %d)\n", after_msg, after_msg - before_msg);
    
    return 0;
}

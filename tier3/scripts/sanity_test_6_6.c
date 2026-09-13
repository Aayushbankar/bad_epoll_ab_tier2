#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <errno.h>

static void print_file_content(const char *path, const char *label) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("[SANITY] %s: open failed (%s): %s\n", label, path, strerror(errno));
        return;
    }
    char buf[512];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n >= 0) {
        buf[n] = '\0';
        // strip trailing newline
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
        printf("[SANITY] %s: %s\n", label, buf);
    } else {
        printf("[SANITY] %s: read error (%s): %s\n", label, path, strerror(errno));
    }
}

int main(void) {
    printf("\n==========================================================\n");
    printf("[*] PHASE 3 SANITY TEST: Linux 6.6.102 GKI (android15-6.6-2025-10_r1)\n");
    printf("==========================================================\n\n");

    // 1. Kernel Version Check
    printf("[1] Checking Kernel Version (/proc/version)...\n");
    print_file_content("/proc/version", "KERNEL VERSION");
    print_file_content("/proc/cmdline", "BOOT CMDLINE");

    // 2. SELinux Status Check
    printf("\n[2] Checking SELinux Status...\n");
    print_file_content("/sys/fs/selinux/enforce", "SELINUX ENFORCE");
    print_file_content("/proc/self/attr/current", "SELINUX CONTEXT (root)");

    // 3. /dev/dma_heap/system Check (Root)
    printf("\n[3] Checking /dev/dma_heap/system (uid=%d)...\n", getuid());
    struct stat st;
    if (stat("/dev/dma_heap/system", &st) == 0) {
        printf("[SANITY] /dev/dma_heap/system mode: 0%o, rdev: %ld\n", st.st_mode & 07777, (long)st.st_rdev);
    } else {
        printf("[SANITY] /dev/dma_heap/system stat failed: %s\n", strerror(errno));
        // Check /sys/class/dma_heap
        if (stat("/sys/class/dma_heap", &st) == 0) {
            printf("[SANITY] /sys/class/dma_heap directory exists in sysfs!\n");
        } else {
            printf("[SANITY] /sys/class/dma_heap does NOT exist (CONFIG_DMABUF_HEAPS_SYSTEM=m or not yet registered; as in stock GKI 6.1/6.6 without vendor heap modules)\n");
        }
    }
    int dma_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_fd >= 0) {
        printf("[SANITY] /dev/dma_heap/system successfully opened as root (fd=%d)\n", dma_fd);
        close(dma_fd);
    } else {
        printf("[SANITY] /dev/dma_heap/system open failed as root: %s\n", strerror(errno));
    }

    // 4. Filp Slab Geometry Check
    printf("\n[4] Checking filp slab cache geometry (/sys/kernel/slab/filp/)...\n");
    print_file_content("/sys/kernel/slab/filp/slabs", "SLABS");
    print_file_content("/sys/kernel/slab/filp/objs_per_slab", "OBJS_PER_SLAB");
    print_file_content("/sys/kernel/slab/filp/object_size", "OBJECT_SIZE");
    print_file_content("/sys/kernel/slab/filp/order", "SLAB_ORDER");

    // 5. Privilege Drop to uid=2000 (shell)
    printf("\n[5] Dropping privileges to uid=2000, gid=2000...\n");
    if (setresgid(2000, 2000, 2000) != 0) {
        perror("[SANITY] setresgid failed");
    }
    if (setresuid(2000, 2000, 2000) != 0) {
        perror("[SANITY] setresuid failed");
    }
    printf("[SANITY] Current UID=%d, EUID=%d, GID=%d, EGID=%d\n", getuid(), geteuid(), getgid(), getegid());

    // Re-check /dev/dma_heap/system as unprivileged user
    dma_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_fd >= 0) {
        printf("[SANITY] /dev/dma_heap/system accessible from uid=%d (fd=%d) [VERIFIED]\n", getuid(), dma_fd);
        close(dma_fd);
    } else {
        printf("[SANITY] /dev/dma_heap/system NOT accessible from uid=%d: %s\n", getuid(), strerror(errno));
    }

    // Re-check /sys/kernel/slab/filp/slabs as unprivileged user
    print_file_content("/sys/kernel/slab/filp/slabs", "SLABS (unprivileged)");

    // 6. Basic fdinfo AAR Sanity Test (Task 7)
    printf("\n[6] Running fdinfo AAR Sanity Test (Task 7, uid=%d)...\n", getuid());
    int pfd[2];
    if (pipe(pfd) != 0) {
        perror("[SANITY] pipe failed");
        return 1;
    }
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("[SANITY] epoll_create1 failed");
        return 1;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = pfd[0];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pfd[0], &ev) != 0) {
        perror("[SANITY] epoll_ctl ADD failed");
        return 1;
    }
    printf("[SANITY] epoll registered pipe fd=%d into epfd=%d\n", pfd[0], epfd);

    char fdinfo_path[64];
    snprintf(fdinfo_path, sizeof(fdinfo_path), "/proc/self/fdinfo/%d", epfd);
    int info_fd = open(fdinfo_path, O_RDONLY);
    if (info_fd < 0) {
        printf("[SANITY] Failed to open %s: %s\n", fdinfo_path, strerror(errno));
    } else {
        char info_buf[2048];
        ssize_t n = read(info_fd, info_buf, sizeof(info_buf) - 1);
        close(info_fd);
        if (n >= 0) {
            info_buf[n] = '\0';
            printf("[SANITY] Raw fdinfo content from %s:\n%s\n", fdinfo_path, info_buf);
            if (strstr(info_buf, "ino:")) {
                printf("[SANITY] SUCCESS: 'ino:' field CONFIRMED in fdinfo output!\n");
            } else {
                printf("[SANITY] WARNING: 'ino:' field not found in fdinfo output!\n");
            }
        } else {
            printf("[SANITY] Failed to read %s: %s\n", fdinfo_path, strerror(errno));
        }
    }

    close(epfd);
    close(pfd[0]);
    close(pfd[1]);

    printf("\n==========================================================\n");
    printf("[*] PHASE 3 SANITY TEST COMPLETE\n");
    printf("==========================================================\n\n");
    return 0;
}

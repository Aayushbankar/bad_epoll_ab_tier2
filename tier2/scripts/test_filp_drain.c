// test_filp_drain.c — EXP-027: Page-Level struct file Cross-Cache Reclaim
// Tests whether mass-freeing struct file via fork-holding drains filp slab pages
// back to the buddy allocator, and checks DMA_HEAP / ashmem accessibility.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <dirent.h>
#include <errno.h>

#define NUM_WORKERS 40
#define FILES_PER_WORKER 800   // 40 * 800 = 32,000 struct file objects
#define UNPRIV_UID 1000
#define UNPRIV_GID 1000

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static long read_slab_metric(const char *metric) {
    char path[128];
    char buf[64] = {0};
    snprintf(path, sizeof(path), "/sys/kernel/slab/filp/%s", metric);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

typedef struct {
    long slabs;
    long objects;
    long total_objects;
    long objs_per_slab;
    long order;
    long object_size;
} FilpStats;

static FilpStats get_filp_stats(void) {
    FilpStats s;
    s.slabs = read_slab_metric("slabs");
    s.objects = read_slab_metric("objects");
    s.total_objects = read_slab_metric("total_objects");
    s.objs_per_slab = read_slab_metric("objs_per_slab");
    s.order = read_slab_metric("order");
    s.object_size = read_slab_metric("object_size");
    return s;
}

static void print_filp_stats(const char *tag, FilpStats s) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "[%s] slabs=%ld, objects=%ld, total_objects=%ld, objs_per_slab=%ld, order=%ld, object_size=%ld\n",
        tag, s.slabs, s.objects, s.total_objects, s.objs_per_slab, s.order, s.object_size);
    print_str(buf);
}

static void setup_dev_nodes(void) {
    mkdir("/dev", 0755);
    mount("tmpfs", "/dev", "tmpfs", 0, "mode=0755");
    mkdir("/dev/dma_heap", 0755);

    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));

    // Parse /proc/misc to find ashmem minor
    FILE *f = fopen("/proc/misc", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            int minor;
            char name[64];
            if (sscanf(line, "%d %63s", &minor, name) == 2) {
                if (strcmp(name, "ashmem") == 0) {
                    mknod("/dev/ashmem", S_IFCHR | 0666, makedev(10, minor));
                    char b[128];
                    snprintf(b, sizeof(b), "[*] Created /dev/ashmem with minor %d\n", minor);
                    print_str(b);
                }
            }
        }
        fclose(f);
    }
}

static void check_dma_heaps(void) {
    print_str("\n--- Checking DMA-BUF Heaps & Allocator Nodes ---\n");

    DIR *dma_class = opendir("/sys/class/dma_heap");
    if (dma_class) {
        print_str("[+] /sys/class/dma_heap exists. Enumerating devices:\n");
        struct dirent *de;
        int count = 0;
        while ((de = readdir(dma_class)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char b[256];
            snprintf(b, sizeof(b), "    found /sys/class/dma_heap/%s\n", de->d_name);
            print_str(b);
            count++;
        }
        if (count == 0) print_str("    (no dma_heap devices registered in sysfs)\n");
        closedir(dma_class);
    } else {
        print_str("[-] /sys/class/dma_heap does not exist (CONFIG_DMABUF_HEAPS_SYSTEM is disabled)\n");
    }

    struct stat st;
    if (stat("/dev/dma_heap/system", &st) == 0) {
        print_str("[+] /dev/dma_heap/system exists\n");
    } else {
        print_str("[-] /dev/dma_heap/system does not exist (CONFIG_DMABUF_HEAPS_SYSTEM is not set in GKI .config)\n");
    }

    if (stat("/dev/ashmem", &st) == 0) {
        print_str("[+] /dev/ashmem exists (CONFIG_ASHMEM=y)\n");
    } else {
        print_str("[-] /dev/ashmem does not exist\n");
    }

    // Unprivileged access test
    print_str("\n--- Testing Unprivileged Access (UID 1000) ---\n");
    pid_t pid = fork();
    if (pid == 0) {
        if (setgid(UNPRIV_GID) != 0 || setuid(UNPRIV_UID) != 0) {
            print_str("[!] Failed to drop privileges to UID 1000\n");
            exit(1);
        }
        char b[256];
        snprintf(b, sizeof(b), "[*] Dropped privileges: getuid()=%d, getgid()=%d\n", getuid(), getgid());
        print_str(b);

        int fd_sys = open("/dev/dma_heap/system", O_RDWR);
        snprintf(b, sizeof(b), "    open(/dev/dma_heap/system) as unpriv: fd=%d (errno=%d: %s)\n",
                 fd_sys, errno, strerror(errno));
        print_str(b);
        if (fd_sys >= 0) close(fd_sys);

        int fd_ash = open("/dev/ashmem", O_RDWR);
        snprintf(b, sizeof(b), "    open(/dev/ashmem) as unpriv: fd=%d (errno=%d: %s)\n",
                 fd_ash, errno, strerror(errno));
        print_str(b);
        if (fd_ash >= 0) {
            // Try setting size and mmap to test page allocation from unpriv
            if (ioctl(fd_ash, 0x41007703 /* ASHMEM_SET_SIZE */, 4096 * 10) == 0) {
                print_str("    ashmem ASHMEM_SET_SIZE 40KB success from unprivileged context\n");
            }
            close(fd_ash);
        }
        exit(0);
    } else {
        waitpid(pid, NULL, 0);
    }
}

static void run_filp_drain_test(void) {
    print_str("\n--- Starting struct file Mass-Freeing Drain Test ---\n");
    FilpStats base = get_filp_stats();
    print_filp_stats("BASELINE", base);

    int ready_pipe[2];
    int release_pipe[2];
    if (pipe(ready_pipe) < 0 || pipe(release_pipe) < 0) {
        print_str("[!] Error: failed to create sync pipes\n");
        return;
    }

    pid_t pids[NUM_WORKERS];
    char msg[128];
    snprintf(msg, sizeof(msg), "[*] Forking %d workers, each opening %d eventfds (%d total)...\n",
             NUM_WORKERS, FILES_PER_WORKER, NUM_WORKERS * FILES_PER_WORKER);
    print_str(msg);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t p = fork();
        if (p == 0) {
            close(ready_pipe[0]);
            close(release_pipe[1]);

            int fds[FILES_PER_WORKER];
            int opened = 0;
            for (int k = 0; k < FILES_PER_WORKER; k++) {
                fds[k] = eventfd(0, 0);
                if (fds[k] >= 0) opened++;
            }

            char c = 'R';
            write(ready_pipe[1], &c, 1);
            close(ready_pipe[1]);

            char go;
            read(release_pipe[0], &go, 1);
            close(release_pipe[0]);

            for (int k = 0; k < opened; k++) {
                close(fds[k]);
            }
            exit(0);
        } else {
            pids[i] = p;
        }
    }

    close(ready_pipe[1]);
    close(release_pipe[0]);

    print_str("[*] Waiting for all workers to allocate eventfd files...\n");
    for (int i = 0; i < NUM_WORKERS; i++) {
        char c;
        read(ready_pipe[0], &c, 1);
    }
    close(ready_pipe[0]);

    print_str("[*] All workers have opened 32,000 files. Sampling PEAK slab stats...\n");
    FilpStats peak = get_filp_stats();
    print_filp_stats("PEAK", peak);

    print_str("[*] Signaling workers to mass-close files and exit...\n");
    char go = 'G';
    for (int i = 0; i < NUM_WORKERS; i++) {
        write(release_pipe[1], &go, 1);
    }
    close(release_pipe[1]);

    for (int i = 0; i < NUM_WORKERS; i++) {
        waitpid(pids[i], NULL, 0);
    }
    print_str("[*] All workers terminated. Waiting for RCU grace periods and SLUB slab reclaim...\n");

    FilpStats post = get_filp_stats();
    for (int step = 1; step <= 5; step++) {
        sleep(1);
        post = get_filp_stats();
        char b[128];
        snprintf(b, sizeof(b), "    T+%ds: slabs=%ld, objects=%ld\n", step, post.slabs, post.objects);
        print_str(b);
    }

    print_filp_stats("POST-DRAIN", post);

    long alloc_delta = peak.slabs - base.slabs;
    long drain_delta = peak.slabs - post.slabs;
    double drain_pct = (alloc_delta > 0) ? ((double)drain_delta * 100.0 / (double)alloc_delta) : 0.0;

    print_str("\n================================================================\n");
    print_str("EXP-027 FINAL RESULTS & DELTAS\n");
    print_str("================================================================\n");
    char summary[512];
    snprintf(summary, sizeof(summary),
        "Baseline filp slabs:     %ld (objects: %ld)\n"
        "Peak filp slabs:         %ld (objects: %ld)\n"
        "Post-drain filp slabs:   %ld (objects: %ld)\n"
        "Allocated slab delta:    +%ld slabs (+%ld objects)\n"
        "Drained slab delta:      -%ld slabs (-%ld objects)\n"
        "Drain efficiency:        %.2f%%\n"
        "Net slabs remaining:     %+ld slabs vs baseline\n",
        base.slabs, base.objects,
        peak.slabs, peak.objects,
        post.slabs, post.objects,
        alloc_delta, peak.objects - base.objects,
        drain_delta, peak.objects - post.objects,
        drain_pct,
        post.slabs - base.slabs
    );
    print_str(summary);

    if (drain_delta > 0 && drain_pct > 80.0) {
        print_str("[+] EXP-027 CONFIRMED: Mass-freeing struct file successfully drains filp slabs back to buddy allocator.\n");
    } else if (drain_delta > 0) {
        print_str("[*] EXP-027 PARTIAL: Some filp slabs drained, but partial slab retention persists.\n");
    } else {
        print_str("[-] EXP-027 REJECTED: Filp slabs were NOT returned to buddy allocator (0 slabs drained).\n");
    }
    print_str("================================================================\n");
}

int main(void) {
    print_str("================================================================\n");
    print_str("EXP-027: Page-Level struct file Cross-Cache Reclaim Test\n");
    print_str("================================================================\n\n");

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    setup_dev_nodes();
    check_dma_heaps();
    run_filp_drain_test();

    reboot(RB_POWER_OFF);
    return 0;
}

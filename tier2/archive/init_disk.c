/*
 * init_disk.c — Init that logs harness output to a virtio-blk disk
 * Used for serial-capture-free runs (KASLR-compatible).
 */
#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static void try_write(int fd, const char *msg) {
    write(fd, msg, strlen(msg));
}

int main() {
    char buf[256];

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    
    /* Try to mount the logging disk (virtio-blk at /dev/vda) */
    mkdir("/mnt", 0755);
    int disk_ok = 0;
    
    /* Wait for device to appear */
    for (int i = 0; i < 20; i++) {
        if (access("/dev/vda", F_OK) == 0) {
            if (mount("/dev/vda", "/mnt", "ext4", 0, NULL) == 0) {
                disk_ok = 1;
                break;
            }
        }
        usleep(100000); /* 100ms */
    }
    
    int log_fd = -1;
    if (disk_ok) {
        log_fd = open("/mnt/harness.log", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
    }
    
    if (log_fd >= 0) {
        dup2(log_fd, 1);
        dup2(log_fd, 2);
        try_write(1, "[*] Init: Output directed to /mnt/harness.log on virtio disk\n");
    } else {
        /* Fallback: try console */
        int console_fd = open("/dev/console", O_WRONLY);
        if (console_fd >= 0 && console_fd != 1) {
            dup2(console_fd, 1);
            dup2(console_fd, 2);
            close(console_fd);
        }
        try_write(1, "[*] Init: No disk available, using console\n");
    }

    snprintf(buf, sizeof(buf), "[*] Init: disk_ok=%d log_fd=%d\n", disk_ok, log_fd);
    try_write(1, buf);
    
    int pid = fork();
    if (pid == 0) {
        execl("/harness", "/harness", NULL);
        try_write(2, "[!] execl failed\n");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        
        /* Sync and unmount to flush */
        sync();
        if (disk_ok) {
            /* Close the log before unmount */
            if (log_fd >= 0) {
                /* Reopen console for final message */
                int cfd = open("/dev/console", O_WRONLY);
                if (cfd >= 0) {
                    dup2(cfd, 1);
                    dup2(cfd, 2);
                    close(cfd);
                }
                close(log_fd);
            }
            sync();
            umount("/mnt");
        }
        
        reboot(RB_POWER_OFF);
    }
    
    return 0;
}

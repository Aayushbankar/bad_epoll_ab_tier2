/*
 * init_serial.c — Minimal init that directs all output to /dev/ttyAMA0 (serial console)
 * Used for AND-002 serial-capture runs without GDB.
 */
#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main() {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    
    /* Direct output to serial console (ttyAMA0) — NOT hvc0 */
    int log_fd = open("/dev/ttyAMA0", O_WRONLY | O_SYNC);
    if (log_fd < 0) {
        /* Fallback to console */
        log_fd = open("/dev/console", O_WRONLY | O_SYNC);
    }
    
    if (log_fd >= 0) {
        dup2(log_fd, 1);
        dup2(log_fd, 2);
    }
    
    int fd;
    fd = open("/proc/sys/kernel/kptr_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }
    
    fd = open("/proc/sys/kernel/dmesg_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }

    printf("[*] Init: Starting harness on serial console...\n");
    fflush(stdout);
    
    int pid = fork();
    if (pid == 0) {
        execl("/harness", "/harness", NULL);
        printf("[!] execl failed\n");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        fflush(stdout);
        if (log_fd >= 0) close(log_fd);
        
        reboot(RB_POWER_OFF);
    }
    
    return 0;
}

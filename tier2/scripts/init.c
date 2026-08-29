#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/klog.h>

int main() {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    
    // Ensure fd 0, 1, 2 are connected to /dev/console
    int console_fd = open("/dev/console", O_RDWR);
    if (console_fd >= 0) {
        if (console_fd != 0) dup2(console_fd, 0);
        dup2(console_fd, 1);
        dup2(console_fd, 2);
        if (console_fd > 2) close(console_fd);
    }
    
    int fd;
    fd = open("/proc/sys/kernel/kptr_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }
    
    fd = open("/proc/sys/kernel/dmesg_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }

    printf("[*] Init: Starting harness on /dev/console...\n");
    fflush(stdout);
    
    int pid = fork();
    if (pid == 0) {
        execl("/harness", "/harness", NULL);
        printf("[!] execl failed\n");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        reboot(RB_POWER_OFF);
    }
    
    return 0;
}

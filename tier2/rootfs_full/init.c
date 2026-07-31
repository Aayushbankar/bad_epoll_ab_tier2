#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    
    int fd;
    fd = open("/proc/sys/kernel/kptr_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }
    
    fd = open("/proc/sys/kernel/dmesg_restrict", O_WRONLY);
    if(fd >= 0) { write(fd, "1\n", 2); close(fd); }

    write(1, "[*] Init Starting exp015\n", 25);
    
    sleep(1);
    
    if (fork() == 0) {
        if (execl("/test_exp015", "/test_exp015", NULL) < 0) {
            perror("execl failed");
        }
        return 1;
    }
    wait(NULL);
    
    write(1, "[*] Executed test_exp015.\n", 26);
    
    write(1, "[*] Done. Powering off...\n", 26);
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return 0;
    return 0;
}

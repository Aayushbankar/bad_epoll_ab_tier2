#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main() {
    printf("[*] CUSTOM HARNESS RUNNING\n");
    system("cat /proc/kallsyms | grep \" ep_item_poll\"");

    int fd = open("/proc/config.gz", O_RDONLY);
    if (fd < 0) {
        printf("No config.gz\n");
    } else {
        printf("config.gz found\n");
        close(fd);
    }
    
    // We don't have zcat or grep in the rootfs! 
    // BUT I can just read /proc/kallsyms directly in C.
    FILE *fp = fopen("/proc/kallsyms", "r");
    char line[256];
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, " ep_item_poll")) {
                printf("[KALLSYMS] %s", line);
            }
        }
        fclose(fp);
    }

    fp = fopen("/sys/kernel/btf/vmlinux", "r");
    if (fp) {
        printf("[BTF] Found vmlinux BTF! Size is big, copying to /dev/ttyAMA0 if possible.\n");
        // Actually without bpftool, I can't easily parse BTF in a C program.
        fclose(fp);
    } else {
        printf("[BTF] /sys/kernel/btf/vmlinux NOT FOUND\n");
    }

    return 0;
}

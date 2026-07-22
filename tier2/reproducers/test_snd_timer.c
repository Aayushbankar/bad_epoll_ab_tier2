#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int main() {
    printf("[*] Diagnostic helper for snd_timer_user reachability.\n");
    
    // Create the device node if it doesn't exist (running as root)
    if (mknod("/dev/snd_timer", S_IFCHR | 0666, makedev(116, 33)) < 0) {
        perror("[-] mknod /dev/snd_timer failed");
        // ignore error if it already exists
    }

    printf("[*] Opening /dev/snd_timer...\n");
    int fd = open("/dev/snd_timer", O_RDONLY);
    if (fd < 0) {
        perror("[-] open /dev/snd_timer failed");
        return 1;
    }

    printf("[+] Successfully opened /dev/snd_timer! fd=%d\n", fd);
    
    // Keep it open for a bit to allow GDB inspection
    sleep(2);
    
    close(fd);
    printf("[*] Done.\n");
    return 0;
}

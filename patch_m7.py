import re

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

# Add setrlimit
text = text.replace(
    'int main(int argc, char **argv) {',
    '''#include <sys/resource.h>

int main(int argc, char **argv) {
    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);
'''
)

text = text.replace(
    'for (int i = 0; i < BATCH_A_SIZE; i++) batch_a_fds[i] = open("/dev/null", O_RDONLY);',
    'for (int i = 0; i < BATCH_A_SIZE; i++) { batch_a_fds[i] = open("/dev/null", O_RDONLY); if (batch_a_fds[i] < 0) { printf("[-] Failed BATCH_A %d\\n", i); break; } }'
)

text = text.replace(
    'for (int i = 0; i < BATCH_B_SIZE; i++) batch_b_fds[i] = open("/dev/null", O_RDONLY);',
    'for (int i = 0; i < BATCH_B_SIZE; i++) { batch_b_fds[i] = open("/dev/null", O_RDONLY); if (batch_b_fds[i] < 0) { printf("[-] Failed BATCH_B %d\\n", i); break; } }'
)

text = text.replace(
    'close(batch_a_fds[i]);',
    'if (batch_a_fds[i] >= 0) close(batch_a_fds[i]);'
)

text = text.replace(
    'close(batch_b_fds[i]);',
    'if (batch_b_fds[i] >= 0) close(batch_b_fds[i]);'
)

text = text.replace(
    'ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);',
    'if (ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) { printf("[-] ioctl alloc failed round %d iter %d\\n", r, i); break; }'
)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

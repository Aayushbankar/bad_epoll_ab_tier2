import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_setup = """void setup_dmabuf_devnode() {
    mkdir("/dev/dma_heap", 0755);
    int fd = open("/sys/class/dma_heap/system/dev", O_RDONLY);
    if (fd < 0) {
        fd = open("/sys/devices/virtual/dma_heap/system/dev", O_RDONLY);
    }
    if (fd >= 0) {
        char buf[32] = {0};
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\\0';
            int maj = 0, min = 0;
            if (sscanf(buf, "%d:%d", &maj, &min) == 2) {
                unlink("/dev/dma_heap/system");
                mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(maj, min));
            }
        }
    }
}
"""

code = re.sub(r"void setup_dmabuf_devnode\(\) \{.*?\n\}\n", new_setup, code, flags=re.DOTALL)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

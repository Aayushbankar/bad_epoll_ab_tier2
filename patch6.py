with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text = f.read()

text = text.replace(
    'uint64_t f_ino = aar_read(epA);',
    '''
    char path[256], buf[4096] = {0};
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA);
    int fdinfo_fd = open(path, O_RDONLY);
    '''
)

text = text.replace(
    '''uint64_t aar_read(int epA_fd) {
    char path[256], buf[4096] = {0};
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA_fd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        printf("fdinfo read returned %d. buffer: %s\\n", n, buf);
        if (strstr(buf, "ino:2f72657070617773")) {
            return 0x2f72657070617773;
        }
    }
    return 0;
}''',
    '''uint64_t aar_read(int fd) {
    char buf[4096] = {0};
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        printf("fdinfo read returned %d. buffer: %s\\n", n, buf);
        if (strstr(buf, "ino:2f72657070617773")) {
            return 0x2f72657070617773;
        }
    }
    return 0;
}'''
)

text = text.replace(
    '    char path[256], buf[4096] = {0};\n    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA);\n    int fdinfo_fd = open(path, O_RDONLY);\n    ',
    ''
)

text = text.replace(
    'getpid(); // Enable __fput hook in GDB',
    '''
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA);
    int fdinfo_fd = open(path, O_RDONLY);
    getpid(); // Enable __fput hook in GDB
    '''
)

text = text.replace(
    '    uint64_t f_ino = aar_read(epA);',
    '    uint64_t f_ino = aar_read(fdinfo_fd);'
)

with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text)

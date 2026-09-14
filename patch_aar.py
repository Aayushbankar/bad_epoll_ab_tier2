with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_aar = """uint64_t aar_read(int fd) {
    char buf[256];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) return 0;
    buf[n] = 0;
    char *p = strstr(buf, "ino:\\t");
    if (p) {
        return strtoull(p + 5, NULL, 16);
    }
    return 0;
}"""

new_aar = """uint64_t aar_read(int fd) {
    char buf[256];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) return 0;
    buf[n] = 0;
    // Look for the "ino:" that is NOT preceded by a newline (the target fd)
    // epA's own info has "ino:\\t" at the start of a line. 
    // The target is printed as " pos:0 ino:X sdev:Y"
    char *p = strstr(buf, " ino:");
    if (p) {
        return strtoull(p + 5, NULL, 16);
    }
    return 0;
}"""

text = text.replace(old_aar, new_aar)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

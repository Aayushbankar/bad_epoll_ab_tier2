with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_get = """uint64_t get_kallsyms_address(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    uint64_t addr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            char *space = strchr(line, ' ');
            if (space) {
                *space = 0;
                addr = strtoull(line, NULL, 16);
                break;
            }
        }
    }
    fclose(f);
    return addr;
}"""

new_get = """uint64_t get_kallsyms_address(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    uint64_t addr = 0;
    while (fgets(line, sizeof(line), f)) {
        char sym_type;
        char sym_name[128];
        uint64_t sym_addr;
        if (sscanf(line, "%llx %c %s", &sym_addr, &sym_type, sym_name) == 3) {
            if (strcmp(sym_name, name) == 0) {
                addr = sym_addr;
                break;
            }
        }
    }
    fclose(f);
    return addr;
}"""

text = text.replace(old_get, new_get)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_setup = """void setup_dmabuf_devnode() {
    if (access("/dev/dma_heap/system", F_OK) == 0) return;
    mkdir("/dev", 0755);
    mkdir("/dev/dma_heap", 0755);
    FILE *f = fopen("/sys/class/dma_heap/system/dev", "r");
    if (f) {
        int major, minor;
        fscanf(f, "%d:%d", &major, &minor);
        fclose(f);
        mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(major, minor));
    }
}
"""

code = re.sub(r"void setup_dmabuf_devnode\(\) \{.*?\n\}\n", new_setup, code, flags=re.DOTALL)
code = "#include <sys/sysmacros.h>\n" + code

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

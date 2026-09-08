with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

import_stmt = "#include <sys/resource.h>\\n#include <sys/ioctl.h>"
text = text.replace("#include <sys/ioctl.h>", import_stmt)

rlimit_code = """    struct rlimit rl;
    rl.rlim_cur = 200000;
    rl.rlim_max = 200000;
    setrlimit(RLIMIT_NOFILE, &rl);
    
    int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);"""

text = text.replace("int dma_heap_fd = open(\"/dev/dma_heap/system\", O_RDWR);", rlimit_code)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

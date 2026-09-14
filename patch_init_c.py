with open("tier2/scripts/init.c", "r") as f:
    text = f.read()

if "mount(\"debugfs\"" not in text:
    text = text.replace('mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);', '''mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL);''')

if "chmod(\"/sys/kernel/debug/epoll_uaf\"" not in text:
    text = text.replace('chmod("/dev/dma_heap/system", 0666);', '''chmod("/dev/dma_heap/system", 0666);
    chmod("/sys/kernel/debug/epoll_uaf", 0666);''')

with open("tier2/scripts/init.c", "w") as f:
    f.write(text)

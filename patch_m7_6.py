with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_alloc = """    int batch_a_fds[BATCH_A_SIZE];
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        batch_a_fds[i] = open("/dev/null", O_RDONLY);
    }

    int epA = epoll_create1(0);
    int inner = eventfd(0, EFD_NONBLOCK);"""

new_alloc = """    int epA = epoll_create1(0);

    int batch_a_fds[BATCH_A_SIZE];
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        batch_a_fds[i] = open("/dev/null", O_RDONLY);
    }

    int inner = eventfd(0, EFD_NONBLOCK);"""

text = text.replace(old_alloc, new_alloc)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

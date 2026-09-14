with open("tier2/scripts/exp_m5_gdb.py", "r") as f:
    text = f.read()

text = text.replace(
    'swaps_poll_addr = to_u64(gdb.parse_and_eval("swaps_poll_addr"))',
    'swaps_poll_addr = to_u64(gdb.parse_and_eval("&swaps_poll"))'
)

with open("tier2/scripts/exp_m5_gdb.py", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text2 = f.read()

text2 = text2.replace(
    '''int inner = epoll_create1(0);
    int epC = eventfd(0, EFD_NONBLOCK);
    struct epoll_event evC = { .events = EPOLLIN, .data.fd = epC };
    epoll_ctl(inner, EPOLL_CTL_ADD, epC, &evC);''',
    'int inner = eventfd(0, EFD_NONBLOCK);'
)
text2 = text2.replace('write(epC, &val, 8);', 'write(inner, &val, 8);')

with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text2)

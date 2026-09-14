with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text = f.read()
text = text.replace(
    'int inner = eventfd(0, EFD_NONBLOCK); // eventfd to arm rdllist',
    '''int inner = epoll_create1(0);
    int epC = eventfd(0, EFD_NONBLOCK);
    struct epoll_event evC = { .events = EPOLLIN, .data.fd = epC };
    epoll_ctl(inner, EPOLL_CTL_ADD, epC, &evC);'''
)
text = text.replace('write(inner, &val, 8);', 'write(epC, &val, 8);')
with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text)

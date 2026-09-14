#!/bin/sh
cat /proc/kallsyms | grep -E "__ep_remove$|eventpoll_release_file$| is_file_epoll$" > /kallsyms_epoll.txt

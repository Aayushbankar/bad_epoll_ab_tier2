with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace('printf("[+] Fire complete.\\n");', '''printf("[+] Fire complete.\\n");
        int dbg = open("/sys/kernel/debug/epoll_uaf", O_RDONLY);
        if (dbg >= 0) {
            char buf[32];
            int n = read(dbg, buf, sizeof(buf)-1);
            if (n > 0) {
                buf[n] = 0;
                printf("[*] VERIFICATION: ep_dbg_uaf_detected = %s\\n", buf);
            }
            close(dbg);
        } else {
            printf("[-] Failed to open /sys/kernel/debug/epoll_uaf\\n");
        }''')

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

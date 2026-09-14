with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace('printf("[-] Failed to open /sys/kernel/debug/epoll_uaf\\n");', '''printf("[-] Failed to open /sys/kernel/debug/epoll_uaf\\n");
        fflush(stdout);''')
text = text.replace('printf("[*] VERIFICATION: ep_dbg_uaf_detected = %s\\n", buf);', '''printf("[*] VERIFICATION: ep_dbg_uaf_detected = %s\\n", buf);
                fflush(stdout);''')

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

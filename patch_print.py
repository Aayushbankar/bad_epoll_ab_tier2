with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old = '''        if (f_ino != 0 && f_ino != 3 && f_ino != 0x0072657070617773 && f_ino != 0x2f72657070617773) {
            printf("[!] f_ino changed to %llx in round %d!\\n", (unsigned long long)f_ino, r);
        }'''

new = '''        if (f_ino != 0 && f_ino != 3 && f_ino != 0x0072657070617773 && f_ino != 0x2f72657070617773) {
            printf("[!] f_ino changed to %llx in round %d!\\n", (unsigned long long)f_ino, r);
            // Re-read and print raw buf
            lseek(fdinfo_fd, 0, SEEK_SET);
            char raw[256] = {0};
            read(fdinfo_fd, raw, 255);
            printf("RAW: %s\\n", raw);
        }'''

text = text.replace(old, new)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

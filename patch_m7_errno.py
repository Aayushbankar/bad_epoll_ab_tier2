with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("""            if (ret < 0) {
                printf("[-] ioctl failed at round %d iter %d\\n", r, i);
                break;
            }""", """            if (ret < 0) {
                perror("ioctl failed");
                printf("[-] ioctl failed at round %d iter %d\\n", r, i);
                break;
            }""")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace(
    'bg_fds[i] = open("/dev/null", O_RDONLY);',
    'bg_fds[i] = open("/dev/null", O_RDONLY); if (bg_fds[i] < 0) { printf("[-] Failed to open /dev/null\\n"); break; }'
)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

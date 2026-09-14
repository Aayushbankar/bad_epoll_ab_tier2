with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text = f.read()

text = text.replace(
    'f("[*] epA=%d, inner=%d. Waiting for GDB...\\n", epA, inner);\\n    fflush(stdout);\\n    sleep(1);\\n    printf("[*] Triggering race by closing inner...\\n");',
    'printf("[*] epA=%d, inner=%d. Triggering race by closing inner...\\n", epA, inner);'
)

text = text.replace(
    'printf("[*] epA=%d, inner=%d. Triggering race by closing inner...\\n", epA, inner);',
    'printf("[*] epA=%d, inner=%d. Waiting for GDB...\\n", epA, inner);\n    fflush(stdout);\n    sleep(1);\n    printf("[*] Triggering race by closing inner...\\n");'
)

with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text)

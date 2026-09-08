with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace(
    'chmod("/dev/null", 0666);',
    'mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));\n    chmod("/dev/null", 0666);'
)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

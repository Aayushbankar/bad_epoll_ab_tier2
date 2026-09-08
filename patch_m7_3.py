with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace(
    '// printf("fdinfo read returned %d. buffer: %s\\n", n, buf);',
    'printf("fdinfo read returned %d. buffer: %s\\n", n, buf);'
)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

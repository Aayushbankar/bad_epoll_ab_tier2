with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace('open("/dev/null", O_RDONLY)', 'eventfd(0, 0)')

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

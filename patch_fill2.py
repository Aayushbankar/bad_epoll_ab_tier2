with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("slot < 4096; slot += 256", "slot + 232 <= 4096; slot += 232")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

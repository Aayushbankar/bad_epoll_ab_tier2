with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("return 0;", "while(1) sleep(1); return 0;")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

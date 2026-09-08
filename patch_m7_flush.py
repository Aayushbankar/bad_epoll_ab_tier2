with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("int main() {", "int main() {\\n    setvbuf(stdout, NULL, _IONBF, 0);")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("\\nint main() {\\n    setvbuf(stdout, NULL, _IONBF, 0);", "\\nint main() {\\n    setvbuf(stdout, NULL, _IONBF, 0);")
text = text.replace("\\\\n", "\\n") # Unescape literal \n

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

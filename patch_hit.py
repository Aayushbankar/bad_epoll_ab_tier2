with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("f_ino != 0x0072657070617773", "f_ino != 0x0072657070617773 && f_ino != 0x2f72657070617773")
text = text.replace("if (f_ino == 0x0072657070617773) {", "if (f_ino == 0x0072657070617773 || f_ino == 0x2f72657070617773) {")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("while(1) sleep(1); return 0;", "return 0;")
# Now only replace the LAST return 0; which is in main!
idx = text.rfind("return 0;")
text = text[:idx] + "while(1) sleep(1); return 0;" + text[idx+9:]

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

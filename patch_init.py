with open("tier2/scripts/init.c", "r") as f:
    text = f.read()
text = text.replace("char *args[] = {\"/harness\", NULL};", "char *args[] = {\"/test_dma\", NULL};")
with open("tier2/scripts/init.c", "w") as f:
    f.write(text)

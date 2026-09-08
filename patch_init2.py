with open("tier2/scripts/init.c", "r") as f:
    text = f.read()
text = text.replace("execl(\"/harness\", \"/harness\", NULL);", "execl(\"/test_dma\", \"/test_dma\", NULL);")
with open("tier2/scripts/init.c", "w") as f:
    f.write(text)

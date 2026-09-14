with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text = f.read()

text = text.replace(
    'if (!f) sleep(10);\n    return 0;',
    'if (!f) return 0;'
)
text = text.replace(
    'return 0;\n    char line',
    'char line'
)

with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text)

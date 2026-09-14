with open("tier2/scripts/test_m5_poll.c", "r") as f:
    text = f.read()
text = text.replace('printprintf', 'printf')
with open("tier2/scripts/test_m5_poll.c", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace(
    'bg_fds[i] = eventfd(0, 0);',
    'bg_fds[i] = open("/dev/null", O_RDONLY);'
)

text = text.replace(
    'batch_a_fds[i] = eventfd(0, 0);',
    'batch_a_fds[i] = open("/dev/null", O_RDONLY);'
)

text = text.replace(
    'batch_b_fds[i] = eventfd(0, 0);',
    'batch_b_fds[i] = open("/dev/null", O_RDONLY);'
)

text = text.replace(
    'chmod("/dev/dma_heap/system", 0666);',
    'chmod("/dev/dma_heap/system", 0666);\n    chmod("/dev/null", 0666);'
)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

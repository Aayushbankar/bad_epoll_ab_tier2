with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("int spray_fds[5 * 8192];", "static int spray_fds[5 * 8192];")
text = text.replace("void *spray_pages[5 * 8192];", "static void *spray_pages[5 * 8192];")
text = text.replace("void *mmap_pages[P_ROUNDS_MMAP * PAGES_PER_MMAP];", "static void *mmap_pages[5 * 8192];")

# Also bg_fds might be too big? 4000 * 4 = 16KB. That's fine. But let's make it static just in case.
text = text.replace("int bg_fds[4000];", "static int bg_fds[4000];")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("int batch_a[32], batch_b[32];", "static int batch_a[1000], batch_b[1000];")
text = text.replace("for (int i = 0; i < 32; i++) batch_a[i] = open(\"/dev/null\", O_RDONLY);", "for (int i = 0; i < 1000; i++) batch_a[i] = open(\"/dev/null\", O_RDONLY);")
text = text.replace("for (int i = 0; i < 32; i++) batch_b[i] = open(\"/dev/null\", O_RDONLY);", "for (int i = 0; i < 1000; i++) batch_b[i] = open(\"/dev/null\", O_RDONLY);")
text = text.replace("for (int i = 0; i < 32; i++) close(batch_a[i]);", "for (int i = 0; i < 1000; i++) close(batch_a[i]);")
text = text.replace("for (int i = 0; i < 32; i++) close(batch_b[i]);", "for (int i = 0; i < 1000; i++) close(batch_b[i]);")

old_close = """    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    sleep(1);
    
    // We don't need bg_fds anymore, or we can close them too
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    sleep(1);"""

new_close = """    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    sleep(2);"""

text = text.replace(old_close, new_close)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

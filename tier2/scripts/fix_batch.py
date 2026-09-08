import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

# Replace batch_a[32] and batch_b[32] with 256
code = code.replace("int batch_a[32], batch_b[32];", "int batch_a[256], batch_b[256];")
code = code.replace("for (int i=0; i<32; i++) batch_a[i] = epoll_create1(0);", "for (int i=0; i<256; i++) batch_a[i] = epoll_create1(0);")
code = code.replace("for (int i=0; i<32; i++) batch_b[i] = epoll_create1(0);", "for (int i=0; i<256; i++) batch_b[i] = epoll_create1(0);")
code = code.replace("for (int i=0; i<32; i++) close(batch_a[i]);", "for (int i=0; i<256; i++) close(batch_a[i]);")
code = code.replace("for (int i=0; i<32; i++) close(batch_b[i]);", "for (int i=0; i<256; i++) close(batch_b[i]);")

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

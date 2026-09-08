import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_aar = """    if (n > 0) {
        buf[n] = '\\0';
        char *tfd = strstr(buf, "tfd:");
        if (tfd) {
            char *ptr = strstr(tfd, "ino:");
            if (ptr) return strtoull(ptr + 4, NULL, 16);
        }
    }
"""

code = re.sub(r"    if \(n > 0\) \{.*?    \}", new_aar, code, flags=re.DOTALL)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

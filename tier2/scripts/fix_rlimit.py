import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_main = """#include <sys/resource.h>
int main(int argc, char **argv) {
    struct rlimit rlim;
    rlim.rlim_cur = 65535;
    rlim.rlim_max = 65535;
    setrlimit(RLIMIT_NOFILE, &rlim);
"""

code = code.replace("int main(int argc, char **argv) {", new_main)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

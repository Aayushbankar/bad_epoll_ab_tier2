import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_main = """int main(int argc, char **argv) {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
"""

code = code.replace("int main(int argc, char **argv) {", new_main)
code = "#include <sys/mount.h>\n" + code

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

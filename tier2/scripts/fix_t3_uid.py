import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_main = """int main(int argc, char **argv) {
    if (argc > 1 && atoi(argv[1]) == 3) {
        setresuid(1000, 1000, 1000);
    }
"""

code = code.replace("int main(int argc, char **argv) {", new_main)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

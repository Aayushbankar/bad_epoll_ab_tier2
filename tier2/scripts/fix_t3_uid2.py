import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

# Remove the early setresuid
code = code.replace("if (argc > 1 && atoi(argv[1]) == 3) {\n        setresuid(1000, 1000, 1000);\n    }", "")

# Put it after setup
code = code.replace("printf(\"[*] Buddy sprayed 8192 dma-buf pages.\\n\");", "printf(\"[*] Buddy sprayed 8192 dma-buf pages.\\n\");\n    if (tier == 3) setresuid(1000, 1000, 1000);")

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)

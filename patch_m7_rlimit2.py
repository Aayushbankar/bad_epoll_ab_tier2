with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("#include <sys/resource.h>\\n#include <sys/ioctl.h>", "#include <sys/resource.h>\\n#include <sys/ioctl.h>") # Wait, literal \n was used!
text = text.replace("\\n", "\n")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)

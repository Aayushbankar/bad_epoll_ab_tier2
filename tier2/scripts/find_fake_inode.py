import struct

with open("tier2/android/artifacts/vmlinux", "rb") as f:
    elf = f.read()

# I need to parse ELF or just use readelf to find the .data section.
# Actually, I can just use python to search the kernel memory dump if I had one.

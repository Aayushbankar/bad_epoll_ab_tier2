with open("tier2/docs/PHASE3_QEMU_POC_PROPOSAL.md", "r") as f:
    content = f.read()

new_risk = """| `epi_fget` failure | `f_count` == 0 aborts poll callback | High | Set `f_count=1`, `f_lock=0`, `f_mode=3` in the forged struct file. |
| Configuration mitigations | `CONFIG_BUG_ON_DATA_CORRUPTION` and `CONFIG_CFI_CLANG` are enabled | Medium | We avoid linked list corruption by leaving `refs` untouched in GDB Variant B. `swaps_poll` does not trigger CFI traps because we do not hijack function pointers. |"""

content = content.replace("| QEMU translation fault | Incorrect `f_inode` offset | High | Ensure `f_inode` is strictly at `+184`. |", "| QEMU translation fault | Incorrect `f_inode` offset | High | Ensure `f_inode` is strictly at `+184`. |\n" + new_risk)

with open("tier2/docs/PHASE3_QEMU_POC_PROPOSAL.md", "w") as f:
    f.write(content)

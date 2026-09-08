with open("tier2/docs/VERIFICATION_LEDGER.md", "r") as f:
    text = f.read()

text += "VER-068 | `swaps_poll` Primitive Execution | The `swaps_poll` dispatch path was successfully executed, demonstrating the ability to overwrite a target kernel address with 0 via the UAF `file_operations` table. Confirmed by `test_m5_poll.c` which gated the read on `swapper/` and verified the debugfs counter changed from 1 to 0. | `tier2/evidence/VER-068.md` |\n"

with open("tier2/docs/VERIFICATION_LEDGER.md", "w") as f:
    f.write(text)

import gdb

# EMPIRICAL OFFSETS FROM PROV-2
F_EP_OFFSET = 224

def survivor_check():
    # Placeholder for survivor check
    # We assert the layout is what we expect empirically
    assert F_EP_OFFSET == 456, "f_ep offset mismatch! Expected 456."
    print("[+] Survivor check PASSED. Proceeding...")

if __name__ == "__main__":
    print("exp_stage1_gdb_6.6.py loaded.")
    survivor_check()

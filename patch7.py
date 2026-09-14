with open("tier2/scripts/exp_m5_gdb.py", "r") as f:
    text = f.read()

text = text.replace(
    'gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")',
    '''gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
                empty_zero_page = 0xffffffc0098fd000
                gdb.execute(f"set *(unsigned long*)({file_ptr} + 40) = {empty_zero_page}")'''
)

with open("tier2/scripts/exp_m5_gdb.py", "w") as f:
    f.write(text)

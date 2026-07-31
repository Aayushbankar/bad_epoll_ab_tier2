import gdb
import time

class DummyBreakpoint(gdb.Breakpoint):
    def stop(self):
        print("\\n[*] DUMMY BREAKPOINT HIT!")
        return True # Stop execution!

def setup():
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("set non-stop off")
    
    retries = 30
    while retries > 0:
        try:
            gdb.execute("target remote :1234")
            break
        except gdb.error:
            time.sleep(1)
            retries -= 1
            
    DummyBreakpoint("__arm64_sys_unshare")
    
    gdb.execute("continue")

setup()

import gdb

class DmaHeapIoctlBreak(gdb.Breakpoint):
    def __init__(self):
        super(DmaHeapIoctlBreak, self).__init__("dma_heap_ioctl")
        self.silent = True

    def stop(self):
        ucmd = gdb.parse_and_eval("$x1")
        print(f"[*] dma_heap_ioctl hit! ucmd = {hex(ucmd)}")
        return False

DmaHeapIoctlBreak()
gdb.execute("c")
gdb.execute("quit")

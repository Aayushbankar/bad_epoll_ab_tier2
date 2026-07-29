import gdb

def run():
    gdb.execute("target remote :1234")
    print("=== FILP CACHE ===")
    gdb.execute("print filp_cachep->name")
    gdb.execute("print filp_cachep->size")
    gdb.execute("print filp_cachep->object_size")
    gdb.execute("print filp_cachep->flags")
    print("=== KMALLOC CACHES ===")
    # Just print the first few kmalloc caches sizes
    gdb.execute("print kmalloc_caches[0][7]->name")
    gdb.execute("print kmalloc_caches[0][7]->size")
    gdb.execute("print kmalloc_caches[0][8]->name")
    gdb.execute("print kmalloc_caches[0][8]->size")
    
    gdb.execute("quit")

run()

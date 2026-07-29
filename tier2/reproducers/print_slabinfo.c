#include <stdio.h>
#include <stdlib.h>

int main() {
    system("cat /proc/slabinfo | grep -E 'filp|kmalloc'");
    return 0;
}

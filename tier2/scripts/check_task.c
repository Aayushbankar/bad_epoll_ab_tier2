#include <stddef.h>
#include <stdio.h>

struct task_struct {
    char dummy[0x798];
    char comm[16];
};

int main() {
    return 0;
}

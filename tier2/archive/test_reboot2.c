#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <stdio.h>
int main() {
    printf("HELLO WORLD FROM TEST_REBOOT\n");
    sleep(15);
    reboot(RB_POWER_OFF);
    return 0;
}

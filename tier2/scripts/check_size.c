#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    FILE *f = fopen("tier2/evidence/EXP-027/EXP-027_raw_serial.log", "r");
    char line[256];
    while(fgets(line, sizeof(line), f)) {
        // Just print everything
    }
    return 0;
}

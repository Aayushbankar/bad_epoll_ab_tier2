#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <dirent.h>

int main() {
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    DIR *d = opendir("/sys/kernel/slab");
    struct dirent *dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf("SLAB: %s\n", dir->d_name);
        }
        closedir(d);
    }
    return 0;
}

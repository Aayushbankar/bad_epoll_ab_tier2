#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
int main() {
    DIR *d = opendir("/sys/kernel/slab/filp");
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }
    
    char buf[256];
    int f = open("/sys/kernel/slab/filp/object_size", 0);
    if (f>0) { int n = read(f, buf, 255); buf[n]=0; printf("object_size: %s\n", buf); close(f); }
    f = open("/sys/kernel/slab/filp/size", 0);
    if (f>0) { int n = read(f, buf, 255); buf[n]=0; printf("size: %s\n", buf); close(f); }
    f = open("/sys/kernel/slab/filp/align", 0);
    if (f>0) { int n = read(f, buf, 255); buf[n]=0; printf("align: %s\n", buf); close(f); }
    return 0;
}

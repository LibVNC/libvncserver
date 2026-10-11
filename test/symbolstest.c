#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_libvncserver.so>\n", argv[0]);
        return 1;
    }
    char command[1024];
    snprintf(command, sizeof(command), "readelf -Ws %s | grep -E \" [A-Z]+ +GLOBAL +DEFAULT +[0-9]+ +tj\"", argv[1]);
    int status = system(command);
    if (status == 0) {
        fprintf(stderr, "Error: exported tj* symbols found in libvncserver.so!\n");
        return 1;
    }
    printf("Success: no exported tj* symbols found in libvncserver.so.\n");
    return 0;
}

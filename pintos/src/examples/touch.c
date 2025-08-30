// #include <stdio.h>
// #include <syscall.h>

// #include <stdbool.h>
// #include <stdlib.h>
#include <syscall.h>
#include "lib/user/syscall.h"
#include "lib/stdio.h"



int main(int argc, char *argv[])
{ 
    
    if (argc < 2) {
        printf("Usage: touch <filename>\n");
        return 1;  // failure if no filename
    }
    for(int i=1;i<argc;i++){
    const char *filename = argv[i];

    // Try to create the file (size = 0 bytes)
    if (!create(filename, 0)) {
        // If create fails, it might already exist
        // Try opening it to check if it exists
        int fd = open(filename);
        if (fd < 0) {
            return 1;
        } else {
            close(fd);
        }
    } else {
        printf("%s: created\n", filename);
    }}

    return 0; // success
}
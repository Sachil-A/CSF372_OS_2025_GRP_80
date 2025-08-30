#include <stdio.h>
#include <syscall.h>

#include <stdbool.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
    printf("in main\n");
    if (argc != 2)
    {
        // Handle the error case (e.g., print a usage message)
        return -1; // Exit with a non-zero status for error
    }
    // 2. Get the filename from the arguments.
    const char *filename = argv[1];
    //attempting creating a new file
    bool success = create(filename, 0);
    if (success)
    {
        // The file was created successfully.
        // The assignment notes state that the command should succeed even if the file already exists.
        // This is because the assignment does not require you to handle this case.
       printf ("%s: created\n", filename);
        
        return 0;
    }
    else
    {
        // The file could not be created.
        printf("fail\n");
        return 0;
    }

    return 0;
// if (argc != 2) {
//         puts("usage: touch <filename>");
//         return 1;
//     }
//     const char *filename = argv[1];
//     int fd = open(filename);
//     if (fd < 0) {
//         if (!create(filename, 0)) {
//             puts("touch: cannot create file");
//             return 1;
//         }
//         for (size_t i = 0; filename[i]; i++) putchar(filename[i]);
//         puts(": created");
//     } else {
//         close(fd);
//     }
//     return 0;
}
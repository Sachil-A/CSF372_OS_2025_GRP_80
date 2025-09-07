//assignment 0:shell assignment final commit


// #include <stdio.h>
// #include <syscall.h>

// #include <stdbool.h>
// #include <stdlib.h>
#include <syscall.h>
#include "lib/user/syscall.h"
#include "lib/stdio.h"

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Usage: touch <filename>\n");
        return 1;
    }

    //running loop for multiple argument case
    for (int i = 1; i < argc; i++)
    {
        const char *filename = argv[i];
        //trying to create file
        if (!create(filename, 0))
        {//file creation unsuccessfull

            int fd = open(filename);
            if (fd < 0)
            {//file doesnot exist
                return 1;
            }
            else
            {
                close(fd);
            }
        }
        else
        {//file creation successful
            printf("%s: created\n", filename);
        }
    }

    return 0;//return with success
}
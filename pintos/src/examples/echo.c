#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  int start;

  if (argc == 1){
    printf("\n");
    return 0; // no argument
  }
  
  
  for (i = 1; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}

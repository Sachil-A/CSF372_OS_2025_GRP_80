#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  if (argc == 1){
    return 0; // no argument case
  }
  
  
  for (i = 1; i < argc; i++){
    char *arg = argv[i]; // current token
    int len = 0;
    while (arg[len] != '\0') {
      len++; // finding length of token
    }
    int start = 0, end = len;

    
    if (len >= 2 && arg[0] == '\'' && arg[len - 1] == '\'') {
      start = 1;
      end = len - 1;
    }

    for (int j = start; j < end; j++) {
      if (arg[j] == '\\' && j + 1 < end && arg[j + 1] == 'n') {
        printf("\n");
        j++;   
      } else {
        printf("%c", arg[j]);
      }
    }

      printf(" ");
    
  }

  printf ("\n");

  return EXIT_SUCCESS;
}

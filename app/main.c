#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/lab.h"
#include <readline/readline.h>
#include <readline/history.h>


int main(int argc, char **argv)
{

  int c;

  while ((c = getopt(argc, argv, "v")) != -1) {
    switch (c)
    {
      case 'v': 
        printf("Shell version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
        exit(EXIT_SUCCESS);
        break;
        
      case '?': // case outside a b or c
        if (isprint(optopt))
          fprintf(stderr, "Unknown option `-%c`.\n", optopt);
        else
          fprintf(stderr, "Unknown option character `\\x%x`.\n", optopt);
        return 1;
        break;

      default:
        break;
    }
  }

  char *line;
  using_history();

  while ((line=readline("$"))){
      printf("%s\n",line);
      add_history(line);
      free(line);
  }

  return 0;
}

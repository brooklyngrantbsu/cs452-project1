#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/lab.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>

// Function to change directory
void change_directory(char *path) {
    char *home = getenv("HOME");

    if (path == NULL || strcmp(path, "") == 0) {
        printf("No directory specified. Switching to HOME directory.\n");
        if (home == NULL) { // home note set, set it manually with user info
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL) {
                home = pw->pw_dir; 
            } else {
                printf("Error: cannot find home directory\n");
                return;
            }
        }
        int ret = chdir(home); // go to directory since not NULL anymore
        if (ret != 0) {
            printf("Error changing directory\n");
        } else {
            printf("Successfully changed directory to: '%s'\n", home);
        }
    } else {
        int ret = chdir(path);
        if (ret != 0) {
            printf("Error changing directory\n");
        } else {
            printf("Successfully changed directory to: '%s'\n", path);
        }
    }
}

// Function to print command history
void print_history() {
    printf("Command History: \n");

    HIST_ENTRY **historyList = history_list();
    if (historyList != NULL) {
        for (int i = 0; historyList[i] != NULL; i++) {
            printf("%d: %s\n", i + 1, historyList[i]->line);
        }
    }
}

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
  
  // get prompt, it will be what shows up before typing
  char *prompt = getenv("MY_PROMPT"); // check if env variable exists
  
  if (prompt == NULL) {
      prompt = "$> "; // default if null
  }

  while ((line=readline(prompt))){
    if (*line) {
      add_history(line);

      char *args[3];  // max 3 args
      int i = 0;
      char *token = strtok(line, " ");
      while (token != NULL && i < 2) {
          args[i++] = token;
          token = strtok(NULL, " ");
      }
      args[i] = NULL;

      if (strcmp(args[0], "exit") == 0) {
        exit(0);  // exit with status 0
      } else if (strcmp(args[0], "cd") == 0) {
          change_directory(args[1]);  // change directory
          free(line);
          continue; 
      } else if (strcmp(args[0], "history") == 0) {
          print_history();  // print the cmd history
          free(line);
          continue; 
      } else {
          fprintf(stderr, "Not a valid command.\n");
          free(line);
          continue; 
      }

    }

    free(line);
  }

  return 0;
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/lab.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>

// global vars
struct termios shell_tmodes;
pid_t shell_pgid;
int shell_terminal;

// runs command with args
void runCommand(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed");
        return;
    }

    if (pid == 0) {
        // child
        pid_t child = getpid();
        setpgid(child, child);
        tcsetpgrp(shell_terminal,child);
        signal (SIGINT, SIG_DFL);
        signal (SIGQUIT, SIG_DFL);
        signal (SIGTSTP, SIG_DFL);
        signal (SIGTTIN, SIG_DFL);
        signal (SIGTTOU, SIG_DFL);
        int ret = execvp(args[0], args);

        if (ret == -1) {
            fprintf(stderr, "Exec failed\n");
        }
        exit(EXIT_FAILURE);
    } else {
        // parent
        setpgid(pid, pid);  // put child in own process group
        tcsetpgrp(shell_terminal, pid);  // give child terminal control

        int status;
        waitpid(pid, &status, WUNTRACED); //wait for chlid then run

        // Give terminal control to shell again
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);
        tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
    }
}

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
  signal(SIGINT, SIG_IGN); // Ctrl+C ignore
  signal(SIGQUIT, SIG_IGN); // ctrl+\ ignore
  signal(SIGTSTP, SIG_IGN); // ctrl+Z ignore
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  shell_terminal = STDIN_FILENO;

  if (isatty(shell_terminal)) {
    // Ensure the shell is in the foreground
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
        kill(-shell_pgid, SIGTTIN);  // Stop the shell until it's in the foreground
    }

    signal(SIGINT, SIG_IGN); // Ctrl+C ignore
    signal(SIGQUIT, SIG_IGN); // ctrl+\ ignore
    signal(SIGTSTP, SIG_IGN); // ctrl+Z ignore
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // Set the shell process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        perror("Couldn't put the shell in its own process group");
        exit(1);
    }

    // Take control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);

    // Save the terminal attributes
    tcgetattr(shell_terminal, &shell_tmodes);
}

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
  
  long arg_max = sysconf(_SC_ARG_MAX); // system max arg amount

  char *args[arg_max / sizeof(char *)];

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

      int i = 0;
      char *token = strtok(line, " ");
      while (token != NULL && i < arg_max / sizeof(char *)) {
          args[i++] = token;
          token = strtok(NULL, " ");
      }
      args[i] = NULL;

      // if empty, leave be and continue
      if (i == 0) {
          free(line);
          continue;
      }

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
      }
      
      runCommand(args);
    }

    free(line);
  }

  return 0;
}

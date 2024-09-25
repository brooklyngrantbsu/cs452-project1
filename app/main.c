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


// objects (structs for c)
typedef struct {
    int id;
    pid_t pid;
    char command[300]; // command typed
    char status[8]; // running or done
} Job;

// global vars
struct termios shell_tmodes;
pid_t shell_pgid;
int shell_terminal;

#define MAX_JOBS 100 // 100 is the max jobs for now
Job jobList[MAX_JOBS];
int jobCount = 0;
int nextJobID = 1; // keep track of id to use


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



/*
Job handling start-----------------------------------------
*/

// Function to add a job to the list
void addJob(pid_t pid, char *command) {
    if (jobCount < MAX_JOBS) {
        jobList[jobCount].id = nextJobID++;
        jobList[jobCount].pid = pid;

        snprintf(jobList[jobCount].command, sizeof(jobList[jobCount].command), "%s", command);
        snprintf(jobList[jobCount].status, sizeof(jobList[jobCount].status), "Running"); // set status to running

        printf("[%d] %d %s\n", jobList[jobCount].id, pid, command);
        jobCount++;
    } else {
        fprintf(stderr, "Too many jobs\n");
    }
}

// Function to remove a job from the list
void removeJob(pid_t pid) {
    for (int i = 0; i < jobCount; i++) {
        if (jobList[i].pid == pid) {
            // shifting
            for (int j = i; j < jobCount - 1; j++) {
                jobList[j] = jobList[j + 1];
            }

            jobCount--;
            break;
        }
    }
}

// Function to check for completed background jobs and mark as done if so
void checkForBackgroundJobs() {
    int status;
    pid_t pid;

    for (int i = 0; i < jobCount; i++) {
        pid = waitpid(jobList[i].pid, &status, WNOHANG);

        if (pid > 0) {
            // Process has finished
            snprintf(jobList[i].status, sizeof(jobList[i].status), "Done");
        }
    }
}

void printJobs() {
    for (int i = 0; i < jobCount; i++) {
        if (strcmp(jobList[i].status, "Done") == 0) {
            // done
            printf("[%d] %s\t %s\n", jobList[i].id, jobList[i].status, jobList[i].command);

            // If the job is done, remove it so it is gone from the list
            removeJob(jobList[i].pid);
            i--;
        } else {
            // running still
            printf("[%d] %d %s %s\n", jobList[i].id, jobList[i].pid, jobList[i].status, jobList[i].command);

        }
    }
}

/*
Job handling end-----------------------------------------
*/


// Function to runs command with args
void runCommand(char **args, int bg, char *command) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed");
        return;
    }

    if (pid == 0) {
        // child
        pid_t child = getpid();
        setpgid(child, child);
        if (!bg) {
            tcsetpgrp(shell_terminal, child); // do not give away control
        }
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

        if (bg) {
            // if wanted in background, put there
            addJob(pid, command);
        } else {

            tcsetpgrp(shell_terminal, pid);  // give child terminal control

            int status;
            waitpid(pid, &status, WUNTRACED); //wait for chlid then run

            // Give terminal control to shell again
            tcsetpgrp(shell_terminal, shell_pgid);
            tcgetattr(shell_terminal, &shell_tmodes);
            tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);

        }
    }
}


int main(int argc, char **argv)
{   
    // check if asking for the version

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

  // if not asking for version and starting terminal, continue here

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
    checkForBackgroundJobs(); // while here, check real quick if any bg jobs are finished

    if (*line) {
      add_history(line);

      int putToBackground = 0;
      char *command = strdup(line); // command that was typed

      char *ampersand = strrchr(line, '&');
      if (ampersand) {
          putToBackground = 1;
          *ampersand = '\0';  // get rif of &
          
          while (isspace((unsigned char) line[strlen(line) - 1])) { // remove spaces before & as well since it doesn't matter
              line[strlen(line) - 1] = '\0';
          }
      }


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
      } else if (strcmp(args[0], "jobs") == 0) {
          printJobs(); // print all jobs
          free(line);
          continue;

      }
      
      runCommand(args, putToBackground, command);
      free(command);
    }

    free(line);
  }

  return 0;
}

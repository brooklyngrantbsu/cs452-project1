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
int shell_terminal;

#define MAX_JOBS 100 // 100 is the max jobs for now
Job jobList[MAX_JOBS];
int jobCount = 0;
int nextJobID = 1; // keep track of id to use


char *get_prompt(const char *env) {
    char *prompt = getenv(env);
    if (prompt == NULL) {
        prompt = "shell>"; // default if null
    }
    return strdup(prompt);
}

int change_dir(char **dir) {
    char *home = getenv("HOME");

    if (dir[1] == NULL || strcmp(dir[1], "") == 0) {
        printf("No directory specified. Switching to HOME directory.\n");
        if (home == NULL) { // home note set, set it manually with user info
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL) {
                home = pw->pw_dir; 
            } else {
                printf("Error: cannot find home directory\n");
            }
        }
        int ret = chdir(home); // go to directory since not NULL anymore
        if (ret != 0) {
            printf("Error changing directory\n");
        } else {
            printf("Successfully changed directory to: '%s'\n", home);
            return 0;
        }
    } else {
        int ret = chdir(dir[1]);
        if (ret != 0) {
            printf("Error changing directory\n");
        } else {
            printf("Successfully changed directory to: '%s'\n", dir[1]);
            return 0;
        }
    }
    return -1; // error if it gets here
}

char **cmd_parse(const char *line) {
    char *lineCopy = strdup(line); // dup line
    if (lineCopy == NULL) {
        perror("strdup failed");
        exit(EXIT_FAILURE);
    }

    int tokenCount = 0;
    int tokenCapacity = 10;
    char **args = malloc(tokenCapacity * sizeof(char *));
    if (args == NULL) {
        perror("Malloc failed");
        free(lineCopy);
        exit(EXIT_FAILURE);
    }

    char *token = strtok(lineCopy, " ");
    while (token) {
        if (tokenCount == tokenCapacity) {
            tokenCapacity *= 2;
            args = realloc(args, tokenCapacity * sizeof(char *));
            if (args == NULL) {
                perror("Reallocating failed");
                free(lineCopy);
                exit(EXIT_FAILURE);
            }
        }
        args[tokenCount++] = strdup(token);
        token = strtok(NULL, " ");
    }
    args[tokenCount] = NULL;

    free(lineCopy);
    return args;
}

void cmd_free(char **line) {
    if (line != NULL) {
        for (int i = 0; line[i] != NULL; i++) {
            free(line[i]);
        }
        free(line);
    }
}

char *trim_white(char *line) {
    if (line == NULL || strlen(line) == 0) {
        return line;
    }
    // end whitespace
    int end = strlen(line) - 1;
    while (end >= 0 && isspace((unsigned char)line[end])) {
        line[end] = '\0';
        end--;
    }
    // start whitespace
    char *start = line;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != line) { // if pointers are equal then there was no white space
        memmove(line, start, strlen(start) + 1);
    }

    return line;
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
            printf("[%d] %s\t %s\n", jobList[i].id, jobList[i].status, jobList[i].command);
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
void runCommand(struct shell *sh, char **args, int bg, char *command) {
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
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
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
            tcsetpgrp(shell_terminal, sh->shell_pgid);
            tcgetattr(shell_terminal, &sh->shell_tmodes);
            tcsetattr(shell_terminal, TCSADRAIN, &sh->shell_tmodes);

        }
    }
}


bool do_builtin(struct shell *sh, char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        sh_destroy(sh);  // Call sh_destroy for exit
        return true;
    } else if (strcmp(argv[0], "cd") == 0) {
        change_dir(argv); // change directory
        return true;
    } else if (strcmp(argv[0], "history") == 0) {
        print_history();  // print cmd history
        return true;
    } else if (strcmp(argv[0], "jobs") == 0) {
        printJobs();  // Call printJobs for jobs
        return true;
    }
    return false;  // Return false if the command is not built-in
}


void sh_init(struct shell *sh) {
    signal(SIGINT, SIG_IGN); // Ctrl+C ignore
    signal(SIGQUIT, SIG_IGN); // ctrl+\ ignore
    signal(SIGTSTP, SIG_IGN); // ctrl+Z ignore
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(shell_terminal);

    if (sh->shell_is_interactive) {
        // Ensure the shell is in the foreground
        while (tcgetpgrp(shell_terminal) != (sh->shell_pgid = getpgrp())) {
            kill(-sh->shell_pgid, SIGTTIN);  // Stop the shell until it's in the foreground
        }

        // Set the shell process group
        sh->shell_pgid = getpid();
        if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Take control of the terminal
        tcsetpgrp(shell_terminal, sh->shell_pgid);

        // Save the terminal attributes
        tcgetattr(shell_terminal, &sh->shell_tmodes);
    }
}


void sh_destroy(struct shell *sh) {
    // Clean up and exit
    tcsetattr(shell_terminal, TCSADRAIN, &sh->shell_tmodes);
    exit(0);
}

void parse_args(int argc, char **argv) {
    int c;

    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c)
        {
        case 'v': 
            printf("Shell version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
            exit(EXIT_SUCCESS);
            break;
            
        case '?': // case outside v
            if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c`.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x`.\n", optopt);
            break;

        default:
            break;
        }
    }
}
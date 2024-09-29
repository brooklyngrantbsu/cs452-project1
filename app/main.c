#include "../src/lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <termios.h>



int main(int argc, char **argv)
{   
    // check if asking for the version
    parse_args(argc, argv);
  
    // if not asking for version and starting terminal, continue here
    struct shell sh;
    sh_init(&sh); 

    char *line;
    using_history();
  
    // get prompt, it will be what shows up before typing
    char *prompt = get_prompt("MY_PROMPT"); // check if env variable exists

    while ((line=readline(prompt))) {
      checkForBackgroundJobs(); // while here, check real quick if any bg jobs are finished

      if (line == NULL || *line == '\0' || strspn(line, " \t\n\r") == strlen(line)) {
          free(line);
          continue;
      }

      if (*line) {
        add_history(line);

        int putToBackground = 0;
        char *command = strdup(line); // command that was typed

        char *ampersand = strrchr(line, '&');
        if (ampersand) {
            putToBackground = 1;
            *ampersand = '\0';  // get rif of &
            trim_white(line);
            
        }

        char **args = cmd_parse(line);

        if (!do_builtin(&sh, args)) {
          runCommand(&sh, args, putToBackground, command);
        }

        cmd_free(args);
        free(command);
      }

      free(line);
  }

  free(prompt);
  return 0;
}

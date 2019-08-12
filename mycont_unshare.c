#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <sys/wait.h>


#define PROG_NAME "mycon"

/*
First Container Engine where we seek for isolation of the nodename. We use fork/execvp
*/

int run_command(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    /* We are in the child here */
    /* set nodename isolation. Fun fact:
       NEWUTS is the achronym for NEW Unix Time Share.
     */
    unshare(CLONE_NEWUTS);

    if (execvp(args[0], args) == -1) {
      perror(PROG_NAME);
      return EXIT_FAILURE;
    }
  }
  else if (pid < 0) {
    // Error forking
    perror(PROG_NAME);
    return EXIT_FAILURE;
  } else {
    // Parent process
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 0;
}



int main(int argc, char* argv[]) {

  return run_command(&argv[1]);
}


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];    /* Space for child's stack */


static int
childFunc(void *arg)
{
    char **argv = arg;
    execvp(argv[0], &argv[0]);
    perror("execvp");
}


int
run_command(char *argv[])
{
  pid_t child_pid;
  int flags = 0;

  // Let's define namespace our program will run into
  flags |= CLONE_NEWPID | CLONE_NEWUTS;

  // Explain here why clone is different from fork (amof clone is called by fork)
  child_pid = clone(childFunc,
                    child_stack + STACK_SIZE,
                    flags | SIGCHLD, &argv[1]);

  if (child_pid == -1)
    perror("clone");

  printf("%s: PID of child created by clone() is %ld\n", argv[0], (long) child_pid);

  /* Parent falls through to here */

  if (waitpid(child_pid, NULL, 0) == -1)      /* Wait for child */
    perror("waitpid");

  exit(0);
}



int main(int argc, char* argv[]) {

  int status;
  run_command(argv);

  return 0;
}
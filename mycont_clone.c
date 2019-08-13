#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <wait.h>

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];    /* Space for child's stack */


static int
child_func(void *arg)
{
    char **argv = arg;
    int status = execvp(argv[0], &argv[0]);
    if (status < 0){
      perror("execvp");
    }
    return status;
}


int
run_command(char *argv[])
{
  pid_t child_pid;
  int flags = 0;

  // Let's define some namespaces for our program will run into
  flags |= CLONE_NEWPID | CLONE_NEWUTS;

  // Explain here why clone is different from fork
  child_pid = clone(child_func,
                    child_stack + STACK_SIZE,
                    flags | SIGCHLD, &argv[0]);

  if (child_pid == -1){
    perror("clone");
    return -1;
  }

  printf("%s: PID of child created by clone() is %ld\n", argv[0], (long) child_pid);

  /* Parent falls through to here and wait
     for child to terminate.
   */

  if (waitpid(child_pid, NULL, 0) == -1)
    perror("waitpid");

  return 0;
}


int main(int argc, char* argv[]) {

  return run_command(&argv[1]);
}

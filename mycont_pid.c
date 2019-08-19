#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>

#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


#define STACK_SIZE (1024 * 1024)
#define NO_FLAGS 0

static char child_stack[STACK_SIZE];    /* Space for child's stack */


int
my_procfs() {
  char* mount_point = "proc2";
  mkdir(mount_point, 0555);

  umount2("/proc", MNT_DETACH);
  if (mount(mount_point, "/proc", "proc", NO_FLAGS, NULL) < 0){
    perror("mount");
    return -1;
  }

  printf("Mounting procfs %s on /proc\n", mount_point);
  return 0;
}


static int
child_func(void *arg)
{
    char **argv = arg;
    int status = my_procfs();
    if (status < 0){
      return -1;
    }
    status = execvp(argv[0], &argv[0]);
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

  // Let's define namespace our program will run into
  flags |= CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS;

  // Explain here why clone is different from fork (amof clone is called by fork)
  child_pid = clone(child_func,
                    child_stack + STACK_SIZE,
                    flags | SIGCHLD, &argv[1]);


  if (child_pid == -1)
    perror("clone");

  printf("%s: PID of child created by clone() is %ld\n", argv[0], (long) child_pid);

  /* Parent falls through to here */

  if (waitpid(child_pid, NULL, 0) == -1)      /* Wait for child */
    perror("waitpid");

  return 0;
}


int
main(int argc, char* argv[]) {

  int status;
  run_command(argv);

  return 0;
}

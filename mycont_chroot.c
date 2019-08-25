#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];


static int
setup_rootfs()
{

  int res = 0;
  res = chroot("rootfs");
  if (res < 0){
    perror("chroot");
    return -1;
  }


  res = mount("tmpfs","/dev","tmpfs", MS_NOSUID | MS_STRICTATIME,NULL);
  if (res < 0){
    perror("mount");
    return -1;
  }

  res = mount("proc", "/proc", "proc",0, NULL);
  if (res < 0){
    perror("mount");
    return -1;
  }
  chdir("/");
  return res;
}


static int
child_func(void *arg)
{
  char **argv = arg;
  char *environ[32]; environ[31] = NULL;

  int res = setup_rootfs();

  printf("Running conatiner [%s]: PID %ld\n", argv[0], (long) getpid());
  execvpe(argv[0], &argv[0], environ);

  // We get here only if execvpe fails to execute!
  perror("execvpe");
}


int
run_command(char *argv[])
{
  pid_t child_pid;
  int flags = 0;

  // Let's define namespaces our program will run with
  flags |= CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS;

  // Explain here why clone is different from fork (aamof clone is called by fork)
  child_pid = clone(child_func,
                    child_stack + STACK_SIZE,
                    flags | SIGCHLD, &argv[1]);


  if (child_pid == -1){
    perror("clone");
    return -1;
  }

  /* Parent falls through to here waiting for child to finish*/
  if (waitpid(child_pid, NULL, 0) == -1){
    perror("waitpid");
    return -1;
  }

  return 0;
}

int main(int argc, char* argv[])
{
  int status = run_command(argv);

  return status;
}

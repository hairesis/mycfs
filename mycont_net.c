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

/*
setup_interface adds a new virtual interface on the host OS
as a bridge for the device inside the container.
*/
int
setup_network_interface(pid_t pid)
{
  char cmd[100];
  system("ip link add veth0 type veth peer name veth1");

  sprintf(cmd, "ip link set veth1 netns %d", pid);
  system(cmd);
  system("ip link set veth0 up");
  system("ip addr add 169.254.1.1/30 dev veth0");
  return 0;
}


int
setup_container_network()
{
  system("ip link set lo up");
  system("ip link set veth1 up");
  system("ip addr add 169.254.1.2/30 dev veth1");
  system("route add default gw 169.254.1.1 veth1");
  return 0;
}


static int
child_func(void *arg)
{
  char **argv = arg;
  char *environ[32]; environ[31] = NULL;

  setup_rootfs();
  setup_container_network();

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
  flags |= CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNET;

  // Explain here why clone is different from fork (aamof clone is called by fork)
  child_pid = clone(child_func,
                    child_stack + STACK_SIZE,
                    flags | SIGCHLD, &argv[1]);

  int res = setup_network_interface(child_pid);
  if (res < 0)
    perror("interface");

  if (child_pid == -1)
    perror("clone");

  /* Parent falls through to here waiting for child to finish*/
  if (waitpid(child_pid, NULL, 0) == -1)
    perror("waitpid");

  return 0;
}

int main(int argc, char* argv[])
{
  int status = run_command(argv);

  return status;
}

# mycfs: My Containers From Scratch

My hope for this work is to be useful to the reader who wishes to have a better understanding on the topic of containerization and make sense of the overwhelming amount of information available in the internet.
As the Reference section illustrates, this is not a novelty and is not intended to be one. This documentation has been a useful tool to my purpose of studying and learning many of the key concepts around containerization and so I wish it can be as much for anyone landing to this page. Search no more.

## Containers Building Blocks
### Kernel
The kernel is the core of a computer's operating system, with complete control over process scheduling and lifecycle (creation and termination), memory management, filesystem, device access, networking, system call APIs.
These are some of the tasks that a Kernel performs on a UNIX-like operating system kernel like Linux.

The Linux Kernel executable is typically called `vmlinuz`. This name comes from the UNIX days when an new version of the kernel `unix` (that's how it was called) implemented virtual memory: `vmunix`. In Linux, the z in the end, indicates that it is a compressed binary.

Containers as in  docker containers or the implementation presented in this document, all share the same "host" kernel.

### System Calls
A system call is a way for a process to request the kernel to perform actions on its behalf. Kernels provide a range of services available to programs through system calls APIs. From a programming point of view, system calls can be called like a function or even from terminal.

Examples of system calls are:
- fork: creates a new process by duplicating the calling process
- exec*: executes the program pointed to by filename
- open: open a file specified by a filename
- close: close a file descriptor
- getpid: return process id
- sethostname: set node hostname
- ...
the list goes on. `man 2 syscalls` lists all system calls available on Linux systems.

### Processes
A process is an instance of an executing program. A process can create a new process using the `fork` system call; such a process is called _parent_ process. The newly created process is called `child` which  typically uses a function of the `exec` family to load an entirely new program in memory. Every new process has an incremental identifier (starting from 1, being typically init or systemd on modern Linux distributions) called PID (process identifier).
A process terminates by calling _exit_ system call or by some other process via a _signal_. When a process exits, the SIGCHLD signal is sent to the `parent`.
### The /proc filesystem
The /proc file system is a virtual file system that provides an easy way to access various processes information under the `/proc/$PID` subdirectory. The program `ps`, for example, uses the /proc virtual file system to retrieve information. For example, `ps -p 1234` is equivalent at looking into `/proc/1234/`.

### Environment Variables
Each process has an _environment list_, which is a list of `envirnoment variables`. Environment variables are inherited by the child as a result of the _fork_ call or can be specified in the _exec_ call. For example *HOME* specifies the path of user's home directory, *PATH* specifies a colon separated list of directories the shell can search for executables.
Type `env` in a shell to see all the instantiated environment variables for the running shell process.

### Shell
A shell reads commands from the user and executes them. There are many shell implementations however, the most used one are: _sh_ and _bash_. Sh, is the oldest of the most common shells, and was written by Steve Bourne. Bourn again Shell (Bash) is the GNU project to replace sh. On many GNU/Linux distributions, sh is implemented by bash, so typing bash or sh at the beginning of a _shell script_ does not make much of a difference nowdays. Bash also conforms to POSIX.

### Directory Hierarchy
The kernel provides a hierarchical directory structure to organize all files in the system at which base is  the  root  directory,  named  / (slash).
A typical directory structure of a Linux system is organized as below:
```
/
├── bin
├── boot
├── dev
├── etc
├── home
├── lib
├── media
├── mnt
├── opt
├── proc
├── root
├── run
├── sbin
├── srv
├── sys
├── tmp
├── usr
└── var
```

### Namespaces
A namespace (NS) _wraps_ some global system resource to provide resource isolation. Linux currently supports various namespaces: each one isolates some kind of resource:
- Mount: isolates mount point list
- UTS: isolates system identifiers such as `hostname`.
- IPC: isolates inter-process communication
- PID: isolates pid number space
- Network: isolates network resources such as firewall rules, port numbers ...
- User: isolates user and group ids
- Cgroups: isolates cgroup pathnames

The idea of namespaces might be confusing if we think about namespaces as in dns names or namespaces programming languages. A key concept is that at any time a given process resides in one namespace instance for each namespace type.
When a new process is created i.e. by using `fork`, it will reside within the same parent's set of namespaces. In order to move a process in another namespace, to isolate it for example into a new process id space, a command or a system call need to be called.
To work with namespaces, programs can use system call `unshare` to create a new namespace and move caller into it.

### Cgroups
TBD

### Capabilities (subset powers of root)
Traditionally, Unix privilege model divides users into two groups: normal users and _root_ (typically with UID 0).
In the modern world (Containers, Kubernetes and Co.) this is clearly a problem since we might want a process to be able to mount a new folder but not be able to change system time or just own the entire system! Capabilities divides root power into a number of pieces.

Examples:
    - *CAP_SYS_CHROOT*: grants a process the capability to use chroot
    - *CAP_SETFCAP*: grants a process the capability to set capabilities!
    - ...
`man 7 capabilities` will give a complete overview.

Setting capabilities to a process, is done through system call `setcap`.


## My first container

Naming is always the hardest part
  -- Any Software Engineer

### Why not in GO?
Docker, as we all know it, is written in golang. Golang is a great language, however the goal here is to understand the quirks of containers as close as possible to the kernel, so C seemed the appropriate way to do it. Also, if you are a fresh graduate, chances are that C is only language you really (think you) know.


### My First Container
#### Fork/Exec
Containers are running processes. Fork (line 5) returns a process identifier for the newly created process. At this point in time such a process (child) is an exact clone of the _parent_.
When the process identifier value returned by fork is 0, it means that the code is being executed in the child. Execution of `execvp` substitutes, the code of the child from what was in the parent with what's specified by the arguments passed from command line, in our case the command `sh`.

```C
int run_command(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
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
```

#### unshare
Let's now move the current process into a new dedicated namespace: UTS.
As illustrated above, processes can be _moved_ into new namespaces using the `unshare` command. In the child code, just before the `exec` command, the unshare system call will do the trick.

```C
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
```

Compile, run and it will appear that nothing has happened!
```bash
$$ make mycont_unshare
cc     mycont_unshare.c   -o mycont_unshare

$ ./mycont_unshare sh
```
until we check the current running processes in the shell:
```bash
$ ps
  PID TTY          TIME CMD
 3587 pts/4    00:00:00 mycont_unshare
 3588 pts/4    00:00:00 sh
 3596 pts/4    00:00:00 ps
31417 pts/4    00:00:00 bash
```
#### Change hostname
As changing hostname is an action for which Super User permissions are required, let's run the program again as root.
[![asciicast](https://asciinema.org/a/D5VIT403nXx5DE8M5ChFhoPQP.svg)](https://asciinema.org/a/D5VIT403nXx5DE8M5ChFhoPQP)


## Unshare vs Clone
The _clone_ system call is a generic interface for the creation of new threads and processes and it is also used to implement fork.
On Linux, at the time of thread creation or process (there is no distinction in the Linux Kernel) using the clone system call, applications can selectively choose which resources to share between threads through the CLONE_* flags.

The System call _unshare_ allows a process to disassociate (hence the name) parts of its execution context that are currently being shared with other processes. unshare thus adds a feature for applications that would like to control shared resources without creating a new process.
The unshare interface, proposed in [kernel.org, 2006](https://www.kernel.org/doc/html/latest/userspace-api/unshare.html), adds a new system call piggy-backing on the _clone_ interface; such a design decision may result in some confusion, given the naming of the flags passed to the _unshare_ system call, are the same passed to the _clone_ one.

To summarise, fork/unshare/exec are very similar to clone/exec, with the difference that the former operates on the caller process, disassociating the namespaces without having to create a new process or thread, while the latter isolates into new namespaces at process creation.

For the purpose of this work, since we are interested in creating new processes in isolation, we are going to refactor our code using `clone`.

```C
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
  flags |= CLONE_NEWUTS;


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
```
Note how the call to the _clone_ function adds an extra SIGCHLD to the flags. The low byte of the flags, contains the number of the termination signal sent to the parent when the child dies.
Since clone is a generic function, we have to specify the termination signal; if no signal is specified the parent process will not be signaled when the child terminates.
As explained in `man 2 clone`, the function requires a pointer to another function to run in the child. In our case, we will run the command we get in input from the client.

## Isolating the pid namespace
ID namespaces isolate the process ID number space, meaning that processes in different PID namespaces can have the same PID.
To create a new process in a isolated number space, the caller process will need *CAP_SYS_ADMIN* capability set or be called by _root_ user. Trying to run code below without being _root_ would result in a *EPERM* error. The first process created in a new namespace will get process id 1.
Let's modify code above to include the new flag *CLONE_NEWPID* to the list of flags and than build it.

For the impatient: full code [here](mycont_pid.c).

```C
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
```

Compile with `make mycont_pid` and run as root:
```bash
# ./mycont_pid sh
# ps
 3587 pts/4    00:00:00 mycont_pid
 3588 pts/4    00:00:00 sh
 3596 pts/4    00:00:00 ps
31417 pts/4    00:00:00 bash
```
The result is not the one we were expecting! This is because _ps_ command looks up into the pseudo file system _/proc_ which we are still sharing with the rest of the system.

## Dedicated /proc filesystem

To make our system looks like more a real container, we have to provide a dedicated /proc filesystem.
To do so, a new function, _my_procfs_ will take care of creating a new _proc2_ folder local to the execution of _mycont_pid_ to substitute itself to the system /proc. This time, our sh command will run in it's dedicated ps namespace and ps will behave as expected.

### Mount namespace and mount propagation

The "mount namespace" of a process is just the set of mounted filesystems that it sees.

This goes from the traditional situation of having one global mount namespace to having per-process mount namespaces. It is thus possible to decide what to do when creating a child process with clone() passing the CLONE_NEWNS (or using unshare) as we've already done in the previous section of this document.

Using CLONE_NEWNS, the child can then unmount /proc, mount another version of it, and only it (and its children) could see the changes.
No other process can see the changes made by the child in this case.

#### Shared Subtrees: mount propagation
After the implementation of mount namespaces was completed, experience showed that the isolation that the implementers provided was, in some cases, too great. For this reason, shared subtrees have been introduced such that each mount-point can then have a different propagation types: shared, private, slave, unbindable.

The Linux kernel defaults the root mount with a private propagation, however modern _systmed_-based systems have the propagation strategy set to _shared_, since it is the more commonly employed propagation type. Needless to say, it's controversial.

A clone() operation with CLONE_NEWNS flag set from a shared mount-point will then result in a counter-intuitive situation of having a container which changes in the new mount namespace take effect on the host system.

A simple way around this behaviour is to set the origin mount point mount propagation to private using the command `mount --make-rprivate /`. This change has effect on the host and can be reverted by restarting systemd daemon or with a system restart.

```C
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
```
`my_procfs` function illustrates the necessary steps required to swap the host procfs filesystem with a container-dedicated one.
The key elements of this approach is to umount the host proc filesystem from the mount view of the child and mount the new (proc2) filesystem as replacement.
Note that, as stated above, this code will have effect on the host system unless the root mount propagation is *private*.

[![asciicast](https://asciinema.org/a/2TAx7uQ0QhqoXWfoERyKr7wEy.svg)](https://asciinema.org/a/2TAx7uQ0QhqoXWfoERyKr7wEy)

The container now has its own view of the process namespace. Listing the /proc directory also shows only two processes (first column) with 1 (sh) and 6 (ls).

## An entire isolated root file system
Although we have now a dedicated /proc file system, we still share everything else there is on the host disk.

Let's download and prepare it:
```bash
$ curl http://dl-cdn.alpinelinux.org/alpine/v3.10/releases/x86_64/alpine-minirootfs-3.10.1-x86_64.tar.gz
$ mkdir rootfs
$ tar xzf alpine-minirootfs-3.10.1-x86_64.tar.gz rootfs
```

What we have to do at this point is to change the root directory of the calling process to the path where the rootfs has been unpacked. To do so *chroot* can be used.
For the impatient: full code [here](mycont_chroot.c).

```C
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

```

### Root Swap vs Chroot


# Resources
- The Linux Programming Interface - Micheal Kerrisk
- https://www.youtube.com/watch?v=Utf-A4rODH8&t=1021s
- https://github.com/tejom/container
- http://man7.org/conf/osseu2017/understanding_user_namespaces-OSS.eu-2017-Kerrisk.pdf
- https://ericchiang.github.io/post/containers-from-scratch/

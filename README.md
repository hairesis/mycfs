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
As illustrated above, processes can be moved into new namespaces using the `unshare` command. In the child code, just before the `exec` command, the unshare system call will do the trick.

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
```bash
$ hostname
fedora
$ sudo ./mycont_unshare sh
$ whoami
root
$ hostname container0
$hostname
container0
$ exit
```
Now exit the container (CTRL+D or type exit) and checking again the hostname will give a pleasant surprise.



# Resources
- The Linux Programming Interface - Micheal Kerrisk
- https://www.youtube.com/watch?v=Utf-A4rODH8&t=1021s
- https://github.com/tejom/container
- http://man7.org/conf/osseu2017/understanding_user_namespaces-OSS.eu-2017-Kerrisk.pdf
- https://ericchiang.github.io/post/containers-from-scratch/

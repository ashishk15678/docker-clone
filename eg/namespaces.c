#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)

void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

// The function that the child process will execute
int child_main(void *arg) {

  // We are now in the child process, inside the new namespaces.
  printf("Child process PID (inside new namespace): %d\n", getpid());

  // Step 1: Set a new hostname inside the UTS namespace.
  printf("1. Setting new hostname to 'my-container-host'...\n");
  if (sethostname("my-container-host", 17) == -1) {
    die("sethostname");
  }
  printf("New hostname is: %s\n", "my-container-host");

  // Step 2: Create a new mount namespace by making mounts private.
  // This ensures mounts made here don't affect the parent's filesystem.
  printf("2. Creating a private mount namespace...\n");
  if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
    die("mount MS_PRIVATE");
  }

  // Create a temporary directory to use as the new root.
  const char *new_root = "/tmp/new-root";
  if (mkdir(new_root, 0755) == -1 && errno != EEXIST) {
    die("mkdir new_root");
  }

  // Step 3: Pivot the root filesystem to the new directory.
  // This is the key step to creating a containerized filesystem view.
  printf("3. Pivoting root to a temporary directory...\n");
  if (mount("tmpfs", new_root, "tmpfs", 0, NULL) == -1) {
    die("mount tmpfs");
  }
  if (mkdirat(AT_FDCWD, "/tmp/new-root/old-root", 0755) == -1) {
    die("mkdir old-root");
  }
  if (syscall(SYS_pivot_root, new_root, "/tmp/new-root/old-root") == -1) {
    die("pivot_root");
  }

  // Unmount the old root.
  if (chdir("/") == -1) {
    die("chdir /");
  }
  if (umount2("/old-root", MNT_DETACH) == -1) {
    die("umount old-root");
  }
  if (rmdir("/old-root") == -1) {
    die("rmdir old-root");
  }

  // Step 4: Execute a new process (e.g., a shell) inside the isolated
  // environment.
  printf("4. Executing /bin/bash...\n");
  execlp("/bin/bash", "bash", (char *)NULL);
  die("execlp"); // Should not be reached
}

int main() {
  char *stack;
  pid_t child_pid;

  // Allocate stack for the child process.
  stack = malloc(STACK_SIZE);
  if (!stack) {
    die("malloc");
  }

  printf("Parent process PID: %d\n", getpid());

  printf("Creating new namespaces for child process...\n");
  child_pid = clone(child_main, stack + STACK_SIZE,
                    CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, NULL);
  if (child_pid == -1) {
    free(stack);
    die("clone");
  }

  // Parent waits for the child to exit.
  if (waitpid(child_pid, NULL, 0) == -1) {
    die("waitpid");
  }

  printf("Child process with PID %d exited. Parent is cleaning up.\n",
         child_pid);

  free(stack);

  return 0;
}

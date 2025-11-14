#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CGROUP_BASE_PATH "/sys/fs/cgroup"
#define CGROUP_NAME "my_cgroup_example"
#define MEMORY_LIMIT_BYTES "10000000" // 10MB limit

void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int main() {
  char cgroup_path[256];
  char subtree_control_path[256];
  char procs_path[256];
  char limit_path[256];
  int fd;
  pid_t pid;

  // Construct the cgroup paths
  snprintf(cgroup_path, sizeof(cgroup_path), "%s/%s", CGROUP_BASE_PATH,
           CGROUP_NAME);
  snprintf(subtree_control_path, sizeof(subtree_control_path),
           "%s/cgroup.subtree_control", CGROUP_BASE_PATH);
  snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);
  snprintf(limit_path, sizeof(limit_path), "%s/memory.max", cgroup_path);

  // Step 1: Enable the 'memory' controller for the root cgroup.
  // This allows child cgroups (like ours) to use the memory controller.
  printf("1. Enabling 'memory' controller for the root cgroup...\n");
  fd = open(subtree_control_path, O_WRONLY);
  if (fd == -1) {
    die("open cgroup.subtree_control");
  }
  if (write(fd, "+memory", strlen("+memory")) == -1) {
    close(fd);
    die("write to cgroup.subtree_control");
  }
  close(fd);

  // Step 2: Create our new cgroup directory.
  printf("2. Creating cgroup directory at: %s\n", cgroup_path);
  if (mkdir(cgroup_path, 0755) == -1) {
    if (errno != EEXIST) {
      die("mkdir");
    }
  }

  // We write to memory.max, the equivalent of memory.limit_in_bytes in v1.
  printf("3. Setting memory limit to %s bytes...\n", MEMORY_LIMIT_BYTES);
  fd = open(limit_path, O_WRONLY);
  if (fd == -1) {
    die("open memory.max");
  }
  if (write(fd, MEMORY_LIMIT_BYTES, strlen(MEMORY_LIMIT_BYTES)) == -1) {
    close(fd);
    die("write to memory.max");
  }
  close(fd);

  // Step 4: Add the current process to the new cgroup.
  // We write the PID to cgroup.procs, the equivalent of tasks in v1.
  printf("4. Adding current PID to the new cgroup...\n");
  pid = getpid();
  char pid_str[32];
  snprintf(pid_str, sizeof(pid_str), "%d", pid);

  fd = open(procs_path, O_WRONLY);
  if (fd == -1) {
    die("open cgroup.procs");
  }
  if (write(fd, pid_str, strlen(pid_str)) == -1) {
    close(fd);
    die("write to cgroup.procs");
  }
  close(fd);

  printf("5. This process is now limited to %s bytes of memory.\n",
         MEMORY_LIMIT_BYTES);
  printf("Attempting to allocate more than the limit...\n");

  // This large allocation will be killed by the kernel's OOM killer
  // due to the cgroup limit.
  size_t big_allocation = 50 * 1024 * 1024; // 50MB
  char *big_chunk = (char *)malloc(big_allocation);

  if (big_chunk == NULL) {
    printf("Failed to allocate memory as expected, process likely killed by "
           "OOM killer.\n");
  } else {
    printf("Memory allocation succeeded (this should not happen if the limit "
           "is enforced).\n");
    free(big_chunk);
  }

  // In a real application, you would perform cleanup here,
  // like removing the cgroup directory.

  rmdir(cgroup_path);
  return 0;
}

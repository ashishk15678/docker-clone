#include "core/daemon.c"
#include <linux/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#define DEMON "Dock-daemon"

int main() {

  // daemon already alive
  if (is_process_running(DEMON)) {
    return EXIT_SUCCESS;
  }

  if (prctl(PR_SET_NAME, DEMON, 0, 0, 0) < 0) {
    perror("Can't set process name");
  }

  while (1 > 0) {
  }
}

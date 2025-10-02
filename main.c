#include "./core/cli-parser.h"
#include "./core/daemon.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#define DEMON "Dock-daemon"

int main(int argc, char *argv[]) {
  if (is_process_running(DEMON)) {
    printf("Daemon running.");
  } else {
  }
}

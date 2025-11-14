#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/prctl.h>

#define DAEMON_NAME "docker-clone-daemon"
#define DAEMON_PORT 2375

// Function declarations
int start_daemon();
int stop_daemon();
int is_daemon_running(const char* host, int port);
int is_process_running(const char *process_name);
int launch_detached_program(const char *path, const char *arg, const char *file_name);
void daemon_signal_handler(int sig);
int daemon_main();

#endif // DAEMON_H

#include "daemon.h"
#include "http.h"
#include "container.h"
#include "image.h"
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int start_daemon() {
    pid_t pid;
    
    // Check if daemon is already running
    if (is_daemon_running("127.0.0.1", DAEMON_PORT)) {
        printf("Daemon is already running\n");
        return 0;
    }
    
    // Fork process
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid > 0) {
        // Parent process
        printf("Daemon started with PID %d\n", pid);
        return 0;
    }
    
    // Child process - become daemon
    if (setsid() < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }
    
    // Second fork to ensure we're not a session leader
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Set process name
    if (prctl(PR_SET_NAME, DAEMON_NAME, 0, 0, 0) < 0) {
        perror("prctl");
    }
    
    // Redirect standard file descriptors
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    
    // Set up signal handlers
    signal(SIGTERM, daemon_signal_handler);
    signal(SIGINT, daemon_signal_handler);
    
    // Initialize systems
    if (init_image_system() != 0) {
        fprintf(stderr, "Failed to initialize image system\n");
        exit(EXIT_FAILURE);
    }
    
    if (init_container_system() != 0) {
        fprintf(stderr, "Failed to initialize container system\n");
        exit(EXIT_FAILURE);
    }
    
    // Start HTTP server
    printf("Docker daemon starting on port %d\n", DAEMON_PORT);
    start_http_server(DAEMON_PORT);
    
    return 0;
}

int stop_daemon() {
    // Find daemon process and send SIGTERM
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pkill -f %s", DAEMON_NAME);
    
    if (system(cmd) == 0) {
        printf("Daemon stopped\n");
        return 0;
    } else {
        printf("Daemon not running or failed to stop\n");
        return -1;
    }
}

// is_daemon_running is implemented in client.c

int is_process_running(const char *process_name) {
#ifndef _WIN32
    DIR *dir;
    struct dirent *entry;
    
    if ((dir = opendir("/proc")) == NULL) {
        perror("opendir /proc failed");
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strspn(entry->d_name, "0123456789") == strlen(entry->d_name)) {
            char path[256];
            FILE *fp;
            char comm_name[256] = {0};
            
            snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
            if ((fp = fopen(path, "r")) != NULL) {
                if (fgets(comm_name, sizeof(comm_name) - 1, fp) != NULL) {
                    comm_name[strcspn(comm_name, "\n")] = 0;
                    if (strcmp(comm_name, process_name) == 0) {
                        fclose(fp);
                        closedir(dir);
                        return 1;
                    }
                }
                fclose(fp);
            }
        }
    }
    
    closedir(dir);
    return 0;
#else
    // Windows implementation would go here
    return 0;
#endif
}

int launch_detached_program(const char *path, const char *arg, const char *file_name) {
    pid_t pid;
    
    pid = fork();
    if (pid < 0) {
        perror("First fork failed");
        return -1;
    }
    
    if (pid > 0) {
        return 0;
    }
    
    if (setsid() < 0) {
        perror("setsid failed");
        exit(EXIT_FAILURE);
    }
    
    pid_t grandchild_pid = fork();
    if (grandchild_pid < 0) {
        perror("Second fork failed");
        exit(EXIT_FAILURE);
    }
    
    if (grandchild_pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    printf("Grandchild (new process) is starting. PID: %d\n", getpid());
    
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    
    if (execlp(path, file_name, arg, NULL) == -1) {
        perror("execlp failed");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}

void daemon_signal_handler(int sig) {
    printf("Daemon received signal %d, shutting down...\n", sig);
    
    // Cleanup
    cleanup_image_system();
    cleanup_container_system();
    
    exit(EXIT_SUCCESS);
}

int daemon_main() {
    return start_daemon();
}
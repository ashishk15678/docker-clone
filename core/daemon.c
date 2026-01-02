#include "daemon.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <unistd.h>

static int wait_for_daemon_ready(const char *host, int port, int retries, useconds_t delay_us);
static int attempt_connect(const char *host, int port);
static int resolve_daemon_binary(char *buffer, size_t len);

int start_daemon() {
    const char *host = DAEMON_HOST;
    char daemon_path[PATH_MAX];

    if (is_daemon_running(host, DAEMON_PORT)) {
        printf("Daemon is already running\n");
        return 0;
    }

    if (resolve_daemon_binary(daemon_path, sizeof(daemon_path)) != 0) {
        fprintf(stderr, "Unable to locate daemon binary\n");
        return -1;
    }

    if (launch_detached_program(daemon_path, NULL, DAEMON_NAME) != 0) {
        fprintf(stderr, "Failed to launch daemon binary\n");
        return -1;
    }

    if (wait_for_daemon_ready(host, DAEMON_PORT, 50, 100000) != 0) {
        fprintf(stderr, "Daemon failed to bind to %s:%d\n", host, DAEMON_PORT);
        return -1;
    }

    printf("Daemon started successfully\n");
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

int is_daemon_running(const char* host, int port) {
    return attempt_connect(host, port) == 0;
}

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

    if (arg && arg[0] != '\0') {
        if (execlp(path, file_name, arg, (char *)NULL) == -1) {
            perror("execlp failed");
            exit(EXIT_FAILURE);
        }
    } else {
        if (execlp(path, file_name, (char *)NULL) == -1) {
            perror("execlp failed");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

static int resolve_daemon_binary(char *buffer, size_t len) {
    if (!buffer || len == 0) {
        return -1;
    }

    ssize_t bytes = readlink("/proc/self/exe", buffer, len - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        char *slash = strrchr(buffer, '/');
        if (slash) {
            *(slash + 1) = '\0';
            strncat(buffer, DAEMON_NAME, len - strlen(buffer) - 1);
            if (access(buffer, X_OK) == 0) {
                return 0;
            }
        }
    }

    if (snprintf(buffer, len, "./build/%s", DAEMON_NAME) < (int)len) {
        if (access(buffer, X_OK) == 0) {
            return 0;
        }
    }

    if (snprintf(buffer, len, "%s", DAEMON_NAME) >= (int)len) {
        return -1;
    }

    return 0;
}

static int wait_for_daemon_ready(const char *host, int port, int retries, useconds_t delay_us) {
    for (int i = 0; i < retries; i++) {
        if (attempt_connect(host, port) == 0) {
            return 0;
        }
        usleep(delay_us);
    }

    return -1;
}

static int attempt_connect(const char *host, int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        close(socket_fd);
        return -1;
    }

    int result = connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    close(socket_fd);
    return result == 0 ? 0 : -1;
}

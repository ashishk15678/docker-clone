#ifndef CONTAINER_H
#define CONTAINER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CONTAINER_NAME_LEN 256
#define MAX_CONTAINER_ID_LEN 64
#define MAX_IMAGE_NAME_LEN 256
#define MAX_COMMAND_LEN 1024
#define MAX_PATH_LEN 512
#define MAX_ENV_VAR_LEN 512
#define MAX_PORT_MAPPING_LEN 64
#define MAX_VOLUME_MAPPING_LEN 256
#define MAX_NETWORK_CONFIG_LEN 256

#define CONTAINER_STORAGE_DIR "/tmp/docker-containers"
#define CONTAINER_METADATA_DIR "/tmp/docker-container-metadata"
#define CONTAINER_LOG_DIR "/tmp/docker-logs"

#define STACK_SIZE (1024 * 1024)

typedef enum {
    CONTAINER_STATE_CREATED,
    CONTAINER_STATE_RUNNING,
    CONTAINER_STATE_PAUSED,
    CONTAINER_STATE_RESTARTING,
    CONTAINER_STATE_REMOVING,
    CONTAINER_STATE_EXITED,
    CONTAINER_STATE_DEAD
} container_state_t;

typedef struct {
    char id[MAX_CONTAINER_ID_LEN];
    char name[MAX_CONTAINER_NAME_LEN];
    char image[MAX_IMAGE_NAME_LEN];
    char image_id[MAX_CONTAINER_ID_LEN];
    char command[MAX_COMMAND_LEN];
    char working_dir[MAX_PATH_LEN];
    char user[MAX_PATH_LEN];
    char shell[MAX_PATH_LEN];
    char entrypoint[MAX_COMMAND_LEN];
    char cmd[MAX_COMMAND_LEN];
    char env_vars[MAX_ENV_VAR_LEN];
    char port_mappings[MAX_PORT_MAPPING_LEN];
    char volume_mappings[MAX_VOLUME_MAPPING_LEN];
    char network_config[MAX_NETWORK_CONFIG_LEN];
    container_state_t state;
    pid_t pid;
    char created[32];
    char started[32];
    char finished[32];
    int exit_code;
    char log_path[MAX_PATH_LEN];
    char rootfs_path[MAX_PATH_LEN];
    char config_path[MAX_PATH_LEN];
    int interactive;
    int tty;
    int detach;
    int restart_policy;
    int memory_limit;
    int cpu_limit;
    int pid_limit;
} container_info_t;

typedef struct {
    container_info_t *containers;
    int count;
    int capacity;
} container_list_t;

// Function declarations
int init_container_system();
int create_container(const char *name, const char *image, const char *command, 
                    const char *working_dir, const char *env_vars, 
                    const char *port_mappings, const char *volume_mappings,
                    int interactive, int tty, int detach);
int start_container(const char *container_id);
int stop_container(const char *container_id);
int restart_container(const char *container_id);
int pause_container(const char *container_id);
int unpause_container(const char *container_id);
int remove_container(const char *container_id);
int exec_container(const char *container_id, const char *command);
container_list_t* list_containers();
container_info_t* get_container_info(const char *container_id);
int container_exists(const char *container_id);
char* generate_container_id();
int create_container_filesystem(const char *container_id, const char *image_id);
int setup_container_namespaces(container_info_t *container);
int setup_container_cgroups(container_info_t *container);
int setup_container_networking(container_info_t *container);
int setup_container_mounts(container_info_t *container);
int cleanup_container_system();

// Container execution functions
int child_main(void *arg);
int run_container_process(container_info_t *container);
int setup_container_environment(container_info_t *container);
int execute_container_command(container_info_t *container);

// Helper functions
char* get_container_full_path(const char *container_id);
int write_container_metadata(container_info_t *container);
int read_container_metadata(const char *container_id, container_info_t *container);
int create_container_directories();
int cleanup_container_resources(const char *container_id);
int signal_container(const char *container_id, int signal);
int get_container_pid(const char *container_id);
int is_container_running(const char *container_id);

// Namespace and cgroup functions
int create_new_namespaces();
int setup_cgroup_limits(container_info_t *container);
int setup_mount_namespace(container_info_t *container);
int setup_network_namespace(container_info_t *container);
int setup_pid_namespace(container_info_t *container);
int setup_uts_namespace(container_info_t *container);
int setup_ipc_namespace(container_info_t *container);

#endif // CONTAINER_H


#include "container.h"
#include <syscall.h>
#include <sched.h>
#include <linux/sched.h>

int init_container_system() {
    if (create_container_directories() != 0) {
        return -1;
    }
    return 0;
}

int create_container_directories() {
    char dirs[][MAX_PATH_LEN] = {
        CONTAINER_STORAGE_DIR,
        CONTAINER_METADATA_DIR,
        CONTAINER_LOG_DIR
    };
    
    for (int i = 0; i < 3; i++) {
        if (mkdir(dirs[i], 0755) != 0 && errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

char* generate_container_id() {
    static char id[MAX_CONTAINER_ID_LEN];
    time_t now = time(NULL);
    snprintf(id, sizeof(id), "cont_%08x%08x", (unsigned int)now, (unsigned int)(now >> 32));
    return id;
}

char* get_container_full_path(const char *container_id) {
    static char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", CONTAINER_STORAGE_DIR, container_id);
    return path;
}

int container_exists(const char *container_id) {
    char metadata_path[MAX_PATH_LEN];
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", CONTAINER_METADATA_DIR, container_id);
    return access(metadata_path, F_OK) == 0;
}

int create_container(const char *name, const char *image, const char *command, 
                    const char *working_dir, const char *env_vars, 
                    const char *port_mappings, const char *volume_mappings,
                    int interactive, int tty, int detach) {
    container_info_t container;
    char container_path[MAX_PATH_LEN];
    
    // Initialize container structure
    memset(&container, 0, sizeof(container));
    strcpy(container.id, generate_container_id());
    strncpy(container.name, name ? name : container.id, sizeof(container.name) - 1);
    strncpy(container.image, image, sizeof(container.image) - 1);
    strncpy(container.command, command ? command : "", sizeof(container.command) - 1);
    strncpy(container.working_dir, working_dir ? working_dir : "/", sizeof(container.working_dir) - 1);
    strncpy(container.env_vars, env_vars ? env_vars : "", sizeof(container.env_vars) - 1);
    strncpy(container.port_mappings, port_mappings ? port_mappings : "", sizeof(container.port_mappings) - 1);
    strncpy(container.volume_mappings, volume_mappings ? volume_mappings : "", sizeof(container.volume_mappings) - 1);
    container.state = CONTAINER_STATE_CREATED;
    container.interactive = interactive;
    container.tty = tty;
    container.detach = detach;
    snprintf(container.created, sizeof(container.created), "%ld", time(NULL));
    
    // Create container directory
    strcpy(container_path, get_container_full_path(container.id));
    if (mkdir(container_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir container");
        return -1;
    }
    
    // Create rootfs directory
    snprintf(container.rootfs_path, sizeof(container.rootfs_path), "%s/rootfs", container_path);
    if (mkdir(container.rootfs_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir rootfs");
        return -1;
    }
    
    // Create log file
    snprintf(container.log_path, sizeof(container.log_path), "%s/%s.log", CONTAINER_LOG_DIR, container.id);
    
    // Create config file path
    snprintf(container.config_path, sizeof(container.config_path), "%s/%s.json", CONTAINER_METADATA_DIR, container.id);
    
    // Write metadata
    if (write_container_metadata(&container) != 0) {
        return -1;
    }
    
    printf("Container %s created successfully\n", container.id);
    return 0;
}

int write_container_metadata(container_info_t *container) {
    FILE *fp;
    
    fp = fopen(container->config_path, "w");
    if (!fp) {
        perror("fopen container metadata");
        return -1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"%s\",\n", container->id);
    fprintf(fp, "  \"name\": \"%s\",\n", container->name);
    fprintf(fp, "  \"image\": \"%s\",\n", container->image);
    fprintf(fp, "  \"image_id\": \"%s\",\n", container->image_id);
    fprintf(fp, "  \"command\": \"%s\",\n", container->command);
    fprintf(fp, "  \"working_dir\": \"%s\",\n", container->working_dir);
    fprintf(fp, "  \"user\": \"%s\",\n", container->user);
    fprintf(fp, "  \"shell\": \"%s\",\n", container->shell);
    fprintf(fp, "  \"entrypoint\": \"%s\",\n", container->entrypoint);
    fprintf(fp, "  \"cmd\": \"%s\",\n", container->cmd);
    fprintf(fp, "  \"env_vars\": \"%s\",\n", container->env_vars);
    fprintf(fp, "  \"port_mappings\": \"%s\",\n", container->port_mappings);
    fprintf(fp, "  \"volume_mappings\": \"%s\",\n", container->volume_mappings);
    fprintf(fp, "  \"network_config\": \"%s\",\n", container->network_config);
    fprintf(fp, "  \"state\": %d,\n", container->state);
    fprintf(fp, "  \"pid\": %d,\n", container->pid);
    fprintf(fp, "  \"created\": \"%s\",\n", container->created);
    fprintf(fp, "  \"started\": \"%s\",\n", container->started);
    fprintf(fp, "  \"finished\": \"%s\",\n", container->finished);
    fprintf(fp, "  \"exit_code\": %d,\n", container->exit_code);
    fprintf(fp, "  \"log_path\": \"%s\",\n", container->log_path);
    fprintf(fp, "  \"rootfs_path\": \"%s\",\n", container->rootfs_path);
    fprintf(fp, "  \"interactive\": %d,\n", container->interactive);
    fprintf(fp, "  \"tty\": %d,\n", container->tty);
    fprintf(fp, "  \"detach\": %d,\n", container->detach);
    fprintf(fp, "  \"restart_policy\": %d,\n", container->restart_policy);
    fprintf(fp, "  \"memory_limit\": %d,\n", container->memory_limit);
    fprintf(fp, "  \"cpu_limit\": %d,\n", container->cpu_limit);
    fprintf(fp, "  \"pid_limit\": %d\n", container->pid_limit);
    fprintf(fp, "}\n");
    
    fclose(fp);
    return 0;
}

int read_container_metadata(const char *container_id, container_info_t *container) {
    char metadata_path[MAX_PATH_LEN];
    FILE *fp;
    char line[1024];
    
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", CONTAINER_METADATA_DIR, container_id);
    
    fp = fopen(metadata_path, "r");
    if (!fp) {
        return -1;
    }
    
    // Simple JSON parsing
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "\"id\"")) {
            sscanf(line, "  \"id\": \"%[^\"]\"", container->id);
        } else if (strstr(line, "\"name\"")) {
            sscanf(line, "  \"name\": \"%[^\"]\"", container->name);
        } else if (strstr(line, "\"image\"")) {
            sscanf(line, "  \"image\": \"%[^\"]\"", container->image);
        } else if (strstr(line, "\"command\"")) {
            sscanf(line, "  \"command\": \"%[^\"]\"", container->command);
        } else if (strstr(line, "\"state\"")) {
            sscanf(line, "  \"state\": %d", &container->state);
        } else if (strstr(line, "\"pid\"")) {
            sscanf(line, "  \"pid\": %d", &container->pid);
        } else if (strstr(line, "\"created\"")) {
            sscanf(line, "  \"created\": \"%[^\"]\"", container->created);
        } else if (strstr(line, "\"started\"")) {
            sscanf(line, "  \"started\": \"%[^\"]\"", container->started);
        } else if (strstr(line, "\"finished\"")) {
            sscanf(line, "  \"finished\": \"%[^\"]\"", container->finished);
        } else if (strstr(line, "\"exit_code\"")) {
            sscanf(line, "  \"exit_code\": %d", &container->exit_code);
        }
    }
    
    fclose(fp);
    return 0;
}

int start_container(const char *container_id) {
    container_info_t container;
    char *stack;
    pid_t child_pid;
    
    if (!container_exists(container_id)) {
        fprintf(stderr, "Container %s does not exist\n", container_id);
        return -1;
    }
    
    if (read_container_metadata(container_id, &container) != 0) {
        fprintf(stderr, "Failed to read container metadata\n");
        return -1;
    }
    
    if (container.state == CONTAINER_STATE_RUNNING) {
        fprintf(stderr, "Container %s is already running\n", container_id);
        return -1;
    }
    
    // Allocate stack for child process
    stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc stack");
        return -1;
    }
    
    printf("Starting container %s...\n", container_id);
    
    // Create new namespaces and start container
    child_pid = clone(child_main, stack + STACK_SIZE,
                     CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, &container);
    
    if (child_pid == -1) {
        perror("clone");
        free(stack);
        return -1;
    }
    
    // Update container metadata
    container.pid = child_pid;
    container.state = CONTAINER_STATE_RUNNING;
    snprintf(container.started, sizeof(container.started), "%ld", time(NULL));
    
    if (write_container_metadata(&container) != 0) {
        kill(child_pid, SIGKILL);
        free(stack);
        return -1;
    }
    
    printf("Container %s started with PID %d\n", container_id, child_pid);
    
    if (container.detach) {
        free(stack);
        return 0;
    } else {
        // Wait for container to exit
        int status;
        waitpid(child_pid, &status, 0);
        
        // Update container state
        container.state = CONTAINER_STATE_EXITED;
        container.exit_code = WEXITSTATUS(status);
        snprintf(container.finished, sizeof(container.finished), "%ld", time(NULL));
        write_container_metadata(&container);
        
        free(stack);
        return 0;
    }
}

int child_main(void *arg) {
    container_info_t *container = (container_info_t *)arg;
    
    printf("Child process PID (inside container): %d\n", getpid());
    
    // Set hostname
    if (sethostname(container->name, strlen(container->name)) == -1) {
        perror("sethostname");
    }
    
    // Setup mount namespace
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        perror("mount MS_PRIVATE");
    }
    
    // Create new root filesystem
    if (mount("tmpfs", container->rootfs_path, "tmpfs", 0, NULL) == -1) {
        perror("mount tmpfs");
    }
    
    // Pivot root
    char old_root[256];
    snprintf(old_root, sizeof(old_root), "%s/old-root", container->rootfs_path);
    
    if (mkdir(old_root, 0755) == -1 && errno != EEXIST) {
        perror("mkdir old-root");
    }
    
    if (syscall(SYS_pivot_root, container->rootfs_path, old_root) == -1) {
        perror("pivot_root");
    }
    
    if (chdir("/") == -1) {
        perror("chdir /");
    }
    
    if (umount2("/old-root", MNT_DETACH) == -1) {
        perror("umount old-root");
    }
    
    if (rmdir("/old-root") == -1) {
        perror("rmdir old-root");
    }
    
    // Execute container command
    if (strlen(container->command) > 0) {
        printf("Executing command: %s\n", container->command);
        execlp("/bin/bash", "bash", "-c", container->command, (char *)NULL);
        perror("execlp");
    } else {
        printf("No command specified, starting shell\n");
        execlp("/bin/bash", "bash", (char *)NULL);
        perror("execlp");
    }
    
    return 0;
}

int stop_container(const char *container_id) {
    container_info_t container;
    
    if (!container_exists(container_id)) {
        fprintf(stderr, "Container %s does not exist\n", container_id);
        return -1;
    }
    
    if (read_container_metadata(container_id, &container) != 0) {
        fprintf(stderr, "Failed to read container metadata\n");
        return -1;
    }
    
    if (container.state != CONTAINER_STATE_RUNNING) {
        fprintf(stderr, "Container %s is not running\n", container_id);
        return -1;
    }
    
    printf("Stopping container %s...\n", container_id);
    
    // Send SIGTERM to container process
    if (kill(container.pid, SIGTERM) != 0) {
        perror("kill SIGTERM");
        return -1;
    }
    
    // Wait for process to exit
    int status;
    if (waitpid(container.pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    
    // Update container state
    container.state = CONTAINER_STATE_EXITED;
    container.exit_code = WEXITSTATUS(status);
    snprintf(container.finished, sizeof(container.finished), "%ld", time(NULL));
    
    if (write_container_metadata(&container) != 0) {
        return -1;
    }
    
    printf("Container %s stopped\n", container_id);
    return 0;
}

int restart_container(const char *container_id) {
    if (stop_container(container_id) != 0) {
        return -1;
    }
    
    return start_container(container_id);
}

int remove_container(const char *container_id) {
    container_info_t container;
    char container_path[MAX_PATH_LEN];
    
    if (!container_exists(container_id)) {
        fprintf(stderr, "Container %s does not exist\n", container_id);
        return -1;
    }
    
    if (read_container_metadata(container_id, &container) != 0) {
        fprintf(stderr, "Failed to read container metadata\n");
        return -1;
    }
    
    if (container.state == CONTAINER_STATE_RUNNING) {
        fprintf(stderr, "Cannot remove running container %s\n", container_id);
        return -1;
    }
    
    printf("Removing container %s...\n", container_id);
    
    // Remove container directory
    strcpy(container_path, get_container_full_path(container_id));
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", container_path);
    if (system(rm_cmd) != 0) {
        fprintf(stderr, "Failed to remove container directory\n");
        return -1;
    }
    
    // Remove metadata file
    if (unlink(container.config_path) != 0) {
        perror("unlink metadata");
        return -1;
    }
    
    // Remove log file
    if (unlink(container.log_path) != 0 && errno != ENOENT) {
        perror("unlink log");
        return -1;
    }
    
    printf("Container %s removed\n", container_id);
    return 0;
}

container_list_t* list_containers() {
    DIR *dir;
    struct dirent *entry;
    container_list_t *list;
    container_info_t *containers;
    int count = 0;
    
    list = malloc(sizeof(container_list_t));
    if (!list) {
        perror("malloc");
        return NULL;
    }
    
    // Count metadata files
    dir = opendir(CONTAINER_METADATA_DIR);
    if (!dir) {
        perror("opendir metadata");
        free(list);
        return NULL;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            count++;
        }
    }
    closedir(dir);
    
    if (count == 0) {
        list->containers = NULL;
        list->count = 0;
        list->capacity = 0;
        return list;
    }
    
    containers = malloc(sizeof(container_info_t) * count);
    if (!containers) {
        perror("malloc");
        free(list);
        return NULL;
    }
    
    // Read metadata files
    dir = opendir(CONTAINER_METADATA_DIR);
    count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            char container_id[MAX_CONTAINER_ID_LEN];
            sscanf(entry->d_name, "%[^.].json", container_id);
            
            if (read_container_metadata(container_id, &containers[count]) == 0) {
                count++;
            }
        }
    }
    closedir(dir);
    
    list->containers = containers;
    list->count = count;
    list->capacity = count;
    
    return list;
}

container_info_t* get_container_info(const char *container_id) {
    container_info_t *container;
    
    container = malloc(sizeof(container_info_t));
    if (!container) {
        perror("malloc");
        return NULL;
    }
    
    if (read_container_metadata(container_id, container) != 0) {
        free(container);
        return NULL;
    }
    
    return container;
}

int is_container_running(const char *container_id) {
    container_info_t container;
    
    if (read_container_metadata(container_id, &container) != 0) {
        return 0;
    }
    
    return container.state == CONTAINER_STATE_RUNNING;
}

int cleanup_container_system() {
    char rm_cmd[1024];
    
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s %s %s", 
             CONTAINER_STORAGE_DIR, CONTAINER_METADATA_DIR, CONTAINER_LOG_DIR);
    
    if (system(rm_cmd) != 0) {
        fprintf(stderr, "Failed to cleanup container system\n");
        return -1;
    }
    
    return 0;
}

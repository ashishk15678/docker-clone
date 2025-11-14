#include "dockerfile.h"

instruction_type_t get_instruction_type(const char *instruction) {
    if (strcmp(instruction, "FROM") == 0) return INSTR_FROM;
    if (strcmp(instruction, "RUN") == 0) return INSTR_RUN;
    if (strcmp(instruction, "CMD") == 0) return INSTR_CMD;
    if (strcmp(instruction, "LABEL") == 0) return INSTR_LABEL;
    if (strcmp(instruction, "EXPOSE") == 0) return INSTR_EXPOSE;
    if (strcmp(instruction, "ENV") == 0) return INSTR_ENV;
    if (strcmp(instruction, "ADD") == 0) return INSTR_ADD;
    if (strcmp(instruction, "COPY") == 0) return INSTR_COPY;
    if (strcmp(instruction, "ENTRYPOINT") == 0) return INSTR_ENTRYPOINT;
    if (strcmp(instruction, "VOLUME") == 0) return INSTR_VOLUME;
    if (strcmp(instruction, "USER") == 0) return INSTR_USER;
    if (strcmp(instruction, "WORKDIR") == 0) return INSTR_WORKDIR;
    if (strcmp(instruction, "ARG") == 0) return INSTR_ARG;
    if (strcmp(instruction, "ONBUILD") == 0) return INSTR_ONBUILD;
    if (strcmp(instruction, "STOPSIGNAL") == 0) return INSTR_STOPSIGNAL;
    if (strcmp(instruction, "HEALTHCHECK") == 0) return INSTR_HEALTHCHECK;
    if (strcmp(instruction, "SHELL") == 0) return INSTR_SHELL;
    return INSTR_UNKNOWN;
}

char* trim_whitespace(char *str) {
    char *end;
    
    // Trim leading space
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    if (*str == 0) {
        return str;
    }
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    end[1] = '\0';
    return str;
}

int is_continuation_line(const char *line) {
    const char *trimmed = trim_whitespace((char*)line);
    return trimmed[0] == '\\' && trimmed[1] == '\0';
}

char* parse_quoted_string(const char *str) {
    static char result[MAX_ARG_LEN];
    const char *start, *end;
    
    start = strchr(str, '"');
    if (!start) {
        strncpy(result, str, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';
        return result;
    }
    
    start++; // Skip opening quote
    end = strrchr(str, '"');
    if (!end || end <= start) {
        strncpy(result, str, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';
        return result;
    }
    
    strncpy(result, start, end - start);
    result[end - start] = '\0';
    return result;
}

dockerfile_t* parse_dockerfile(const char *file_path) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    dockerfile_t *dockerfile;
    dockerfile_instruction_t *instructions;
    int line_number = 0;
    int capacity = 10;
    int count = 0;
    char continuation_buffer[MAX_LINE_LEN] = {0};
    int in_continuation = 0;
    
    fp = fopen(file_path, "r");
    if (!fp) {
        perror("fopen dockerfile");
        return NULL;
    }
    
    dockerfile = malloc(sizeof(dockerfile_t));
    if (!dockerfile) {
        perror("malloc");
        fclose(fp);
        return NULL;
    }
    
    memset(dockerfile, 0, sizeof(dockerfile_t));
    instructions = malloc(sizeof(dockerfile_instruction_t) * capacity);
    if (!instructions) {
        perror("malloc");
        free(dockerfile);
        fclose(fp);
        return NULL;
    }
    
    dockerfile->instructions = instructions;
    dockerfile->capacity = capacity;
    
    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        char *trimmed = trim_whitespace(line);
        
        // Skip empty lines and comments
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }
        
        // Handle continuation lines
        if (is_continuation_line(trimmed)) {
            in_continuation = 1;
            continue;
        }
        
        if (in_continuation) {
            strcat(continuation_buffer, " ");
            strcat(continuation_buffer, trimmed);
            in_continuation = 0;
            trimmed = continuation_buffer;
            memset(continuation_buffer, 0, sizeof(continuation_buffer));
        }
        
        // Parse instruction
        char *space = strchr(trimmed, ' ');
        if (!space) {
            fprintf(stderr, "Invalid instruction at line %d: %s\n", line_number, trimmed);
            continue;
        }
        
        *space = '\0';
        char *instruction = trimmed;
        char *args = space + 1;
        
        instruction_type_t type = get_instruction_type(instruction);
        if (type == INSTR_UNKNOWN) {
            fprintf(stderr, "Unknown instruction at line %d: %s\n", line_number, instruction);
            continue;
        }
        
        // Resize array if needed
        if (count >= capacity) {
            capacity *= 2;
            instructions = realloc(instructions, sizeof(dockerfile_instruction_t) * capacity);
            if (!instructions) {
                perror("realloc");
                free(dockerfile);
                fclose(fp);
                return NULL;
            }
            dockerfile->instructions = instructions;
            dockerfile->capacity = capacity;
        }
        
        // Store instruction
        memset(&instructions[count], 0, sizeof(dockerfile_instruction_t));
        instructions[count].type = type;
        strncpy(instructions[count].instruction, instruction, sizeof(instructions[count].instruction) - 1);
        strncpy(instructions[count].args, args, sizeof(instructions[count].args) - 1);
        instructions[count].line_number = line_number;
        
        // Parse specific instruction arguments
        switch (type) {
            case INSTR_FROM:
                strncpy(dockerfile->base_image, args, sizeof(dockerfile->base_image) - 1);
                break;
            case INSTR_WORKDIR:
                strncpy(dockerfile->working_dir, args, sizeof(dockerfile->working_dir) - 1);
                break;
            case INSTR_USER:
                strncpy(dockerfile->user, args, sizeof(dockerfile->user) - 1);
                break;
            case INSTR_SHELL:
                strncpy(dockerfile->shell, args, sizeof(dockerfile->shell) - 1);
                break;
            case INSTR_ENTRYPOINT:
                strncpy(dockerfile->entrypoint, args, sizeof(dockerfile->entrypoint) - 1);
                break;
            case INSTR_CMD:
                strncpy(dockerfile->cmd, args, sizeof(dockerfile->cmd) - 1);
                break;
            case INSTR_ENV:
                if (dockerfile->env_count < MAX_ENV_VAR_LEN) {
                    dockerfile->env_vars[dockerfile->env_count] = strdup(args);
                    dockerfile->env_count++;
                }
                break;
            case INSTR_EXPOSE:
                if (dockerfile->port_count < MAX_PORT_LEN) {
                    dockerfile->exposed_ports[dockerfile->port_count] = strdup(args);
                    dockerfile->port_count++;
                }
                break;
            case INSTR_VOLUME:
                if (dockerfile->volume_count < MAX_VOLUME_LEN) {
                    dockerfile->volumes[dockerfile->volume_count] = strdup(args);
                    dockerfile->volume_count++;
                }
                break;
            case INSTR_LABEL:
                if (dockerfile->label_count < MAX_ENV_VAR_LEN) {
                    dockerfile->labels[dockerfile->label_count] = strdup(args);
                    dockerfile->label_count++;
                }
                break;
            default:
                break;
        }
        
        count++;
    }
    
    dockerfile->count = count;
    fclose(fp);
    
    return dockerfile;
}

void free_dockerfile(dockerfile_t *dockerfile) {
    if (!dockerfile) return;
    
    if (dockerfile->instructions) {
        free(dockerfile->instructions);
    }
    
    for (int i = 0; i < dockerfile->env_count; i++) {
        free(dockerfile->env_vars[i]);
    }
    
    for (int i = 0; i < dockerfile->port_count; i++) {
        free(dockerfile->exposed_ports[i]);
    }
    
    for (int i = 0; i < dockerfile->volume_count; i++) {
        free(dockerfile->volumes[i]);
    }
    
    for (int i = 0; i < dockerfile->label_count; i++) {
        free(dockerfile->labels[i]);
    }
    
    free(dockerfile);
}

int validate_dockerfile(dockerfile_t *dockerfile) {
    if (!dockerfile) {
        fprintf(stderr, "Dockerfile is NULL\n");
        return 0;
    }
    
    if (dockerfile->count == 0) {
        fprintf(stderr, "Dockerfile is empty\n");
        return 0;
    }
    
    // First instruction must be FROM
    if (dockerfile->instructions[0].type != INSTR_FROM) {
        fprintf(stderr, "First instruction must be FROM\n");
        return 0;
    }
    
    // Check for invalid instruction combinations
    int has_entrypoint = 0, has_cmd = 0;
    for (int i = 0; i < dockerfile->count; i++) {
        if (dockerfile->instructions[i].type == INSTR_ENTRYPOINT) {
            has_entrypoint = 1;
        }
        if (dockerfile->instructions[i].type == INSTR_CMD) {
            has_cmd = 1;
        }
    }
    
    if (has_entrypoint && has_cmd) {
        fprintf(stderr, "Warning: Both ENTRYPOINT and CMD specified\n");
    }
    
    return 1;
}

int build_image_from_dockerfile(dockerfile_t *dockerfile, const char *image_name, const char *tag, const char *context_path) {
    char layer_path[MAX_PATH_LEN];
    char command[MAX_COMMAND_LEN];
    
    if (!validate_dockerfile(dockerfile)) {
        return -1;
    }
    
    // Create base layer from base image
    strcpy(layer_path, "/tmp/docker-build-layer");
    if (mkdir(layer_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir layer");
        return -1;
    }
    
    // Execute each instruction
    for (int i = 0; i < dockerfile->count; i++) {
        dockerfile_instruction_t *instruction = &dockerfile->instructions[i];
        
        printf("Step %d/%d: %s %s\n", i + 1, dockerfile->count, 
               instruction->instruction, instruction->args);
        
        if (execute_instruction(instruction, context_path, layer_path) != 0) {
            fprintf(stderr, "Failed to execute instruction at line %d\n", instruction->line_number);
            return -1;
        }
    }
    
    // Create final image
    snprintf(command, sizeof(command), "Built from Dockerfile");
    if (create_image(image_name, tag, NULL, layer_path) != 0) {
        fprintf(stderr, "Failed to create image\n");
        return -1;
    }
    
    // Cleanup
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", layer_path);
    system(rm_cmd);
    
    return 0;
}

int execute_instruction(dockerfile_instruction_t *instruction, const char *context_path, const char *layer_path) {
    switch (instruction->type) {
        case INSTR_FROM:
            // Base image handling - in a real implementation, this would pull/extract the base image
            printf("Using base image: %s\n", instruction->args);
            break;
            
        case INSTR_RUN:
            return run_command(instruction->args, layer_path);
            
        case INSTR_COPY:
            return copy_files(instruction->args, layer_path, context_path);
            
        case INSTR_ADD:
            return add_files(instruction->args, layer_path, context_path);
            
        case INSTR_WORKDIR:
            return create_working_directory(instruction->args, layer_path);
            
        case INSTR_ENV:
            return set_environment_variable(instruction->args, layer_path);
            
        case INSTR_USER:
            return set_user(instruction->args, layer_path);
            
        case INSTR_EXPOSE:
            return expose_port(instruction->args, layer_path);
            
        case INSTR_VOLUME:
            return create_volume(instruction->args, layer_path);
            
        case INSTR_ENTRYPOINT:
            return set_entrypoint(instruction->args, layer_path);
            
        case INSTR_CMD:
            return set_cmd(instruction->args, layer_path);
            
        case INSTR_LABEL:
            return set_label(instruction->args, layer_path);
            
        default:
            printf("Instruction %s not yet implemented\n", instruction->instruction);
            break;
    }
    
    return 0;
}

int run_command(const char *command, const char *layer_path) {
    char cmd[1024];
    char chroot_cmd[1024];
    
    // Create a simple script to run the command
    snprintf(cmd, sizeof(cmd), "echo '#!/bin/bash\n%s' > %s/run_command.sh", command, layer_path);
    if (system(cmd) != 0) {
        return -1;
    }
    
    snprintf(chroot_cmd, sizeof(chroot_cmd), "chmod +x %s/run_command.sh", layer_path);
    if (system(chroot_cmd) != 0) {
        return -1;
    }
    
    // In a real implementation, this would execute the command in a chroot environment
    printf("Would execute: %s\n", command);
    return 0;
}

int copy_files(const char *args, const char *layer_path, const char *context_path) {
    char *src, *dest;
    char *args_copy = strdup(args);
    
    // Parse source and destination
    src = strtok(args_copy, " ");
    dest = strtok(NULL, " ");
    
    if (!src || !dest) {
        free(args_copy);
        return -1;
    }
    
    int result = copy_files(src, dest, context_path);
    free(args_copy);
    return result;
}

int add_files(const char *args, const char *layer_path, const char *context_path) {
    // ADD is similar to COPY but can handle URLs and tar files
    return copy_files(args, layer_path, context_path);
}

int set_environment_variable(const char *key_value, const char *layer_path) {
    char env_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(env_file, sizeof(env_file), "%s/environment", layer_path);
    fp = fopen(env_file, "a");
    if (!fp) {
        perror("fopen env file");
        return -1;
    }
    
    fprintf(fp, "%s\n", key_value);
    fclose(fp);
    
    return 0;
}

int create_working_directory(const char *path, const char *layer_path) {
    char workdir_path[MAX_PATH_LEN];
    
    snprintf(workdir_path, sizeof(workdir_path), "%s%s", layer_path, path);
    if (mkdir(workdir_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir workdir");
        return -1;
    }
    
    return 0;
}

int set_user(const char *user, const char *layer_path) {
    char user_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(user_file, sizeof(user_file), "%s/user", layer_path);
    fp = fopen(user_file, "w");
    if (!fp) {
        perror("fopen user file");
        return -1;
    }
    
    fprintf(fp, "%s\n", user);
    fclose(fp);
    
    return 0;
}

int expose_port(const char *port, const char *layer_path) {
    char port_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(port_file, sizeof(port_file), "%s/exposed_ports", layer_path);
    fp = fopen(port_file, "a");
    if (!fp) {
        perror("fopen port file");
        return -1;
    }
    
    fprintf(fp, "%s\n", port);
    fclose(fp);
    
    return 0;
}

int create_volume(const char *path, const char *layer_path) {
    char volume_path[MAX_PATH_LEN];
    
    snprintf(volume_path, sizeof(volume_path), "%s%s", layer_path, path);
    if (mkdir(volume_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir volume");
        return -1;
    }
    
    return 0;
}

int set_entrypoint(const char *entrypoint, const char *layer_path) {
    char entrypoint_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(entrypoint_file, sizeof(entrypoint_file), "%s/entrypoint", layer_path);
    fp = fopen(entrypoint_file, "w");
    if (!fp) {
        perror("fopen entrypoint file");
        return -1;
    }
    
    fprintf(fp, "%s\n", entrypoint);
    fclose(fp);
    
    return 0;
}

int set_cmd(const char *cmd, const char *layer_path) {
    char cmd_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(cmd_file, sizeof(cmd_file), "%s/cmd", layer_path);
    fp = fopen(cmd_file, "w");
    if (!fp) {
        perror("fopen cmd file");
        return -1;
    }
    
    fprintf(fp, "%s\n", cmd);
    fclose(fp);
    
    return 0;
}

int set_label(const char *key_value, const char *layer_path) {
    char label_file[MAX_PATH_LEN];
    FILE *fp;
    
    snprintf(label_file, sizeof(label_file), "%s/labels", layer_path);
    fp = fopen(label_file, "a");
    if (!fp) {
        perror("fopen label file");
        return -1;
    }
    
    fprintf(fp, "%s\n", key_value);
    fclose(fp);
    
    return 0;
}

void print_dockerfile_info(dockerfile_t *dockerfile) {
    if (!dockerfile) {
        printf("Dockerfile is NULL\n");
        return;
    }
    
    printf("Dockerfile Information:\n");
    printf("  Base Image: %s\n", dockerfile->base_image);
    printf("  Working Directory: %s\n", dockerfile->working_dir);
    printf("  User: %s\n", dockerfile->user);
    printf("  Shell: %s\n", dockerfile->shell);
    printf("  Entrypoint: %s\n", dockerfile->entrypoint);
    printf("  CMD: %s\n", dockerfile->cmd);
    printf("  Instructions: %d\n", dockerfile->count);
    printf("  Environment Variables: %d\n", dockerfile->env_count);
    printf("  Exposed Ports: %d\n", dockerfile->port_count);
    printf("  Volumes: %d\n", dockerfile->volume_count);
    printf("  Labels: %d\n", dockerfile->label_count);
    
    printf("\nInstructions:\n");
    for (int i = 0; i < dockerfile->count; i++) {
        printf("  %d. %s %s (line %d)\n", i + 1, 
               dockerfile->instructions[i].instruction,
               dockerfile->instructions[i].args,
               dockerfile->instructions[i].line_number);
    }
}


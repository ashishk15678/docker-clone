#include "cli-parser.h"

command_type_t get_command_type(const char *cmd) {
    if (strcmp(cmd, "run") == 0) return CMD_RUN;
    if (strcmp(cmd, "build") == 0) return CMD_BUILD;
    if (strcmp(cmd, "images") == 0) return CMD_IMAGES;
    if (strcmp(cmd, "containers") == 0) return CMD_CONTAINERS;
    if (strcmp(cmd, "stop") == 0) return CMD_STOP;
    if (strcmp(cmd, "rm") == 0) return CMD_RM;
    if (strcmp(cmd, "rmi") == 0) return CMD_RMI;
    if (strcmp(cmd, "ps") == 0) return CMD_PS;
    if (strcmp(cmd, "logs") == 0) return CMD_LOGS;
    if (strcmp(cmd, "exec") == 0) return CMD_EXEC;
    if (strcmp(cmd, "commit") == 0) return CMD_COMMIT;
    if (strcmp(cmd, "daemon") == 0) return CMD_DAEMON;
    return CMD_UNKNOWN;
}

parsed_command_t* parse_arguments(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: No command specified\n");
        print_usage(argv[0]);
        return NULL;
    }

    parsed_command_t *cmd = malloc(sizeof(parsed_command_t));
    if (!cmd) {
        perror("malloc");
        return NULL;
    }

    // clean the memory
    memset(cmd, 0, sizeof(parsed_command_t));

    cmd->type = get_command_type(argv[1]);
    cmd->args = malloc(sizeof(char*) * argc);
    cmd->argc = argc;

    if (!cmd->args) {
        perror("malloc");
        free(cmd);
        return NULL;
    }

    // Copy all arguments
    for (int i = 0; i < argc; i++) {
        cmd->args[i] = strdup(argv[i]);
    }

    // Parse specific command options
    switch (cmd->type) {
        case CMD_RUN:
            parse_run_command(cmd, argc, argv);
            break;
        case CMD_BUILD:
            parse_build_command(cmd, argc, argv);
            break;
        case CMD_STOP:
        case CMD_RM:
        case CMD_RMI:
        case CMD_LOGS:
        case CMD_EXEC:
            parse_container_command(cmd, argc, argv);
            break;
        case CMD_COMMIT:
            parse_commit_command(cmd, argc, argv);
            break;
        default:
            break;
    }

    return cmd;
}

void parse_run_command(parsed_command_t *cmd, int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--detach") == 0) {
            cmd->detach = 1;
        } else if (strcmp(argv[i], "-it") == 0) {
            cmd->interactive = 1;
            cmd->tty = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            cmd->interactive = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tty") == 0) {
            cmd->tty = 1;
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            strncpy(cmd->container_name, argv[++i], sizeof(cmd->container_name) - 1);
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--publish") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->port_mapping, argv[++i], sizeof(cmd->port_mapping) - 1);
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--volume") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->volume_mapping, argv[++i], sizeof(cmd->volume_mapping) - 1);
            }
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--env") == 0) {
            if (i + 1 < argc) {
                if (strlen(cmd->env_vars) > 0) {
                    strcat(cmd->env_vars, ";");
                }
                strcat(cmd->env_vars, argv[++i]);
            }
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--workdir") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->working_dir, argv[++i], sizeof(cmd->working_dir) - 1);
            }
        } else if (argv[i][0] != '-') {
            // This should be the image name or command
            if (strlen(cmd->image_name) == 0) {
                strncpy(cmd->image_name, argv[i], sizeof(cmd->image_name) - 1);
            } else {
                // This is the command to run
                strncpy(cmd->command, argv[i], sizeof(cmd->command) - 1);
                break;
            }
        }
    }
}

void parse_build_command(parsed_command_t *cmd, int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tag") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->image_name, argv[++i], sizeof(cmd->image_name) - 1);
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->dockerfile_path, argv[++i], sizeof(cmd->dockerfile_path) - 1);
            }
        } else if (argv[i][0] != '-') {
            // This should be the build context
            if (strlen(cmd->working_dir) == 0) {
                strncpy(cmd->working_dir, argv[i], sizeof(cmd->working_dir) - 1);
            }
        }
    }

    // Default dockerfile path if not specified
    if (strlen(cmd->dockerfile_path) == 0) {
        strcpy(cmd->dockerfile_path, "Dockerfile");
    }
}

void parse_container_command(parsed_command_t *cmd, int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') {
            strncpy(cmd->container_name, argv[i], sizeof(cmd->container_name) - 1);
            break;
        }
    }
}

void parse_commit_command(parsed_command_t *cmd, int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--message") == 0) {
            if (i + 1 < argc) {
                strncpy(cmd->command, argv[++i], sizeof(cmd->command) - 1);
            }
        } else if (argv[i][0] != '-') {
            if (strlen(cmd->container_name) == 0) {
                strncpy(cmd->container_name, argv[i], sizeof(cmd->container_name) - 1);
            } else if (strlen(cmd->image_name) == 0) {
                strncpy(cmd->image_name, argv[i], sizeof(cmd->image_name) - 1);
            }
        }
    }
}

int validate_command(parsed_command_t *cmd) {
    switch (cmd->type) {
        case CMD_RUN:
            if (strlen(cmd->image_name) == 0) {
                fprintf(stderr, "Error: Image name required for 'run' command\n");
                return 0;
            }
            break;
        case CMD_BUILD:
            if (strlen(cmd->image_name) == 0) {
                fprintf(stderr, "Error: Image name required for 'build' command\n");
                return 0;
            }
            break;
        case CMD_STOP:
        case CMD_RM:
        case CMD_LOGS:
        case CMD_EXEC:
            if (strlen(cmd->container_name) == 0) {
                fprintf(stderr, "Error: Container name required for '%s' command\n",
                        cmd->type == CMD_STOP ? "stop" :
                        cmd->type == CMD_RM ? "rm" :
                        cmd->type == CMD_LOGS ? "logs" : "exec");
                return 0;
            }
            break;
        case CMD_COMMIT:
            if (strlen(cmd->container_name) == 0 || strlen(cmd->image_name) == 0) {
                fprintf(stderr, "Error: Container name and image name required for 'commit' command\n");
                return 0;
            }
            break;
        default:
            break;
    }
    return 1;
}

void free_parsed_command(parsed_command_t *cmd) {
    if (!cmd) return;

    if (cmd->args) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->args[i]);
        }
        free(cmd->args);
    }
    free(cmd);
}

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [options]\n\n", program_name);
    printf("Commands:\n");
    printf("  run        Run a container from an image\n");
    printf("  build      Build an image from a Dockerfile\n");
    printf("  images     List images\n");
    printf("  containers List containers\n");
    printf("  ps         List running containers\n");
    printf("  stop       Stop a running container\n");
    printf("  rm         Remove a container\n");
    printf("  rmi        Remove an image\n");
    printf("  logs       Show container logs\n");
    printf("  exec       Execute command in running container\n");
    printf("  commit     Create image from container\n");
    printf("  daemon     Start the daemon\n\n");
    printf("Examples:\n");
    printf("  %s run -it ubuntu bash\n", program_name);
    printf("  %s build -t myimage .\n", program_name);
    printf("  %s images\n", program_name);
    printf("  %s ps\n", program_name);
}

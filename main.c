#include "./core/cli-parser.h"
#include "./core/client.h"
#include "./core/daemon.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    parsed_command_t *cmd;

    // Parse command line arguments
    cmd = parse_arguments(argc, argv);
    if (!cmd) {
        return EXIT_FAILURE;
    }

    // Validate command
    if (!validate_command(cmd)) {
        free_parsed_command(cmd);
        return EXIT_FAILURE;
    }

    // Handle daemon command
    if (cmd->type == CMD_DAEMON) {
        if (is_daemon_running("127.0.0.1", 2375)) {
            printf("Daemon is already running\n");
        } else {
            printf("Starting daemon...\n");
            if (start_daemon() != 0) {
                fprintf(stderr, "Failed to start daemon\n");
                free_parsed_command(cmd);
                return EXIT_FAILURE;
            }
        }
        free_parsed_command(cmd);
        return EXIT_SUCCESS;
    }

    // Check if daemon is running for other commands
    if (!is_daemon_running("127.0.0.1", 2375)) {
        fprintf(stderr, "Daemon is not running. Please start the daemon first.\n");
        fprintf(stderr, "Use: %s daemon\n", argv[0]);
        free_parsed_command(cmd);
        return EXIT_FAILURE;
    }

    // Execute command
    int result = 0;
    switch (cmd->type) {
        case CMD_RUN:
            result = docker_run(cmd->image_name, cmd->command, cmd->container_name,
                              cmd->working_dir, cmd->env_vars, cmd->port_mapping,
                              cmd->volume_mapping, cmd->interactive, cmd->tty, cmd->detach);
            break;

        case CMD_BUILD:
            result = docker_build(cmd->image_name, cmd->dockerfile_path, cmd->working_dir);
            break;

        case CMD_IMAGES:
            result = docker_images();
            break;

        case CMD_CONTAINERS:
        case CMD_PS:
            result = docker_containers();
            break;

        case CMD_STOP:
            result = docker_stop(cmd->container_name);
            break;

        case CMD_RM:
            result = docker_rm(cmd->container_name);
            break;

        case CMD_RMI:
            result = docker_rmi(cmd->image_name);
            break;

        case CMD_LOGS:
            result = docker_logs(cmd->container_name);
            break;

        case CMD_EXEC:
            result = docker_exec(cmd->container_name, cmd->command);
            break;

        case CMD_COMMIT:
            result = docker_commit(cmd->container_name, cmd->image_name, cmd->command);
            break;

        default:
            fprintf(stderr, "Unknown command\n");
            result = -1;
            break;
    }

    free_parsed_command(cmd);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

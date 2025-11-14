#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_COMMAND_LEN 256
#define MAX_ARGS 32
#define MAX_PATH_LEN 512

typedef enum {
    CMD_UNKNOWN,
    CMD_RUN,
    CMD_BUILD,
    CMD_IMAGES,
    CMD_CONTAINERS,
    CMD_STOP,
    CMD_RM,
    CMD_RMI,
    CMD_PS,
    CMD_LOGS,
    CMD_EXEC,
    CMD_COMMIT,
    CMD_DAEMON
} command_type_t;

typedef struct {
    command_type_t type;
    char **args;
    int argc;
    char image_name[MAX_PATH_LEN];
    char container_name[MAX_PATH_LEN];
    char dockerfile_path[MAX_PATH_LEN];
    char working_dir[MAX_PATH_LEN];
    char command[MAX_COMMAND_LEN];
    int detach;
    int interactive;
    int tty;
    char port_mapping[64];
    char volume_mapping[256];
    char env_vars[512];
} parsed_command_t;

// Function declarations
parsed_command_t* parse_arguments(int argc, char *argv[]);
void free_parsed_command(parsed_command_t *cmd);
void print_usage(const char *program_name);
command_type_t get_command_type(const char *cmd);
int validate_command(parsed_command_t *cmd);
void parse_run_command(parsed_command_t *cmd, int argc, char *argv[]);
void parse_build_command(parsed_command_t *cmd, int argc, char *argv[]);
void parse_container_command(parsed_command_t *cmd, int argc, char *argv[]);
void parse_commit_command(parsed_command_t *cmd, int argc, char *argv[]);

#endif // CLI_PARSER_H

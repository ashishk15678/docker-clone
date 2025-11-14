#ifndef DOCKERFILE_H
#define DOCKERFILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE_LEN 1024
#define MAX_INSTRUCTION_LEN 32
#define MAX_ARG_LEN 512
#define MAX_PATH_LEN 512
#define MAX_ENV_VAR_LEN 256
#define MAX_PORT_LEN 32
#define MAX_VOLUME_LEN 256
#define MAX_LAYER_COUNT 100
#define MAX_COMMAND_LEN 1024

typedef enum {
    INSTR_UNKNOWN,
    INSTR_FROM,
    INSTR_RUN,
    INSTR_CMD,
    INSTR_LABEL,
    INSTR_EXPOSE,
    INSTR_ENV,
    INSTR_ADD,
    INSTR_COPY,
    INSTR_ENTRYPOINT,
    INSTR_VOLUME,
    INSTR_USER,
    INSTR_WORKDIR,
    INSTR_ARG,
    INSTR_ONBUILD,
    INSTR_STOPSIGNAL,
    INSTR_HEALTHCHECK,
    INSTR_SHELL
} instruction_type_t;

typedef struct {
    instruction_type_t type;
    char instruction[MAX_INSTRUCTION_LEN];
    char args[MAX_ARG_LEN];
    char src[MAX_PATH_LEN];
    char dest[MAX_PATH_LEN];
    int line_number;
} dockerfile_instruction_t;

typedef struct {
    dockerfile_instruction_t *instructions;
    int count;
    int capacity;
    char base_image[MAX_PATH_LEN];
    char working_dir[MAX_PATH_LEN];
    char user[MAX_PATH_LEN];
    char shell[MAX_PATH_LEN];
    char entrypoint[MAX_ARG_LEN];
    char cmd[MAX_ARG_LEN];
    char *env_vars[MAX_ENV_VAR_LEN];
    int env_count;
    char *exposed_ports[MAX_PORT_LEN];
    int port_count;
    char *volumes[MAX_VOLUME_LEN];
    int volume_count;
    char *labels[MAX_ENV_VAR_LEN];
    int label_count;
} dockerfile_t;

// Function declarations
dockerfile_t* parse_dockerfile(const char *file_path);
void free_dockerfile(dockerfile_t *dockerfile);
instruction_type_t get_instruction_type(const char *instruction);
int validate_dockerfile(dockerfile_t *dockerfile);
int build_image_from_dockerfile(dockerfile_t *dockerfile, const char *image_name, const char *tag, const char *context_path);
int execute_instruction(dockerfile_instruction_t *instruction, const char *context_path, const char *layer_path);
int copy_files(const char *src, const char *dest, const char *context_path);
int add_files(const char *src, const char *dest, const char *context_path);
int run_command(const char *command, const char *layer_path);
int set_environment_variable(const char *key_value, const char *layer_path);
int create_working_directory(const char *path, const char *layer_path);
int set_user(const char *user, const char *layer_path);
int expose_port(const char *port, const char *layer_path);
int create_volume(const char *path, const char *layer_path);
int set_entrypoint(const char *entrypoint, const char *layer_path);
int set_cmd(const char *cmd, const char *layer_path);
int set_label(const char *key_value, const char *layer_path);

// Helper functions
char* trim_whitespace(char *str);
int is_continuation_line(const char *line);
char* parse_quoted_string(const char *str);
int expand_variables(char *str, const char *env_vars[], int env_count);
void print_dockerfile_info(dockerfile_t *dockerfile);

#endif // DOCKERFILE_H

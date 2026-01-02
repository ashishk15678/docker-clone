#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"

#define MAX_RESPONSE_SIZE 8192
#define MAX_REQUEST_SIZE 4096
#define DEFAULT_DAEMON_PORT DOCKERD_PORT
#define DEFAULT_DAEMON_HOST DOCKERD_HOST

// Function declarations
int connect_to_daemon(const char* host, int port);
int send_request_to_daemon(int socket, const char* method, const char* url, const char* body);
int receive_response_from_daemon(int socket, char* response, int max_size);
int docker_run(const char* image, const char* command, const char* name, 
               const char* working_dir, const char* env_vars, 
               const char* port_mappings, const char* volume_mappings,
               int interactive, int tty, int detach);
int docker_build(const char* image_name, const char* dockerfile_path, const char* context_path);
int docker_images();
int docker_containers();
int docker_ps();
int docker_stop(const char* container_id);
int docker_rm(const char* container_id);
int docker_rmi(const char* image_name);
int docker_logs(const char* container_id);
int docker_exec(const char* container_id, const char* command);
int docker_commit(const char* container_id, const char* image_name, const char* message);
int docker_version();
int docker_info();

// Helper functions
int create_http_request(char* request, const char* method, const char* url, const char* body);
int parse_http_response(const char* response, int* status_code, char* body);
void print_containers_json(const char* json);
void print_images_json(const char* json);

#endif // CLIENT_H


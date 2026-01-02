#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "config.h"

#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 8192
#define MAX_HEADER_SIZE 1024
#define MAX_URL_SIZE 512
#define MAX_METHOD_SIZE 16
#define MAX_VERSION_SIZE 16
#define MAX_THREADS 10

#define DEFAULT_PORT DOCKERD_PORT
#define DEFAULT_HOST DOCKERD_HOST

typedef struct {
    char method[MAX_METHOD_SIZE];
    char url[MAX_URL_SIZE];
    char version[MAX_VERSION_SIZE];
    char headers[MAX_HEADER_SIZE];
    char body[MAX_REQUEST_SIZE];
    int content_length;
} http_request_t;

typedef struct {
    char version[MAX_VERSION_SIZE];
    int status_code;
    char status_message[64];
    char headers[MAX_HEADER_SIZE];
    char body[MAX_RESPONSE_SIZE];
    int content_length;
} http_response_t;

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

// Function declarations
int start_http_server(int port);
void* handle_client(void* arg);
int parse_http_request(const char* request, http_request_t* parsed);
int create_http_response(http_response_t* response, int status_code, const char* status_message, const char* body);
int send_http_response(int client_socket, http_response_t* response);
int handle_api_request(http_request_t* request, http_response_t* response);
int handle_containers_api(http_request_t* request, http_response_t* response);
int handle_images_api(http_request_t* request, http_response_t* response);
int handle_container_create(http_request_t* request, http_response_t* response);
int handle_container_start(http_request_t* request, http_response_t* response);
int handle_container_stop(http_request_t* request, http_response_t* response);
int handle_container_remove(http_request_t* request, http_response_t* response);
int handle_image_build(http_request_t* request, http_response_t* response);
int handle_image_list(http_request_t* request, http_response_t* response);
int handle_image_remove(http_request_t* request, http_response_t* response);
void cleanup_server(int server_socket);

// Helper functions
char* url_decode(const char* str);
char* url_encode(const char* str);
int extract_container_id_from_url(const char* url, char* container_id);
int extract_image_name_from_url(const char* url, char* image_name);
void log_request(http_request_t* request, struct sockaddr_in* client_addr);
void log_response(http_response_t* response);

#endif // HTTP_H

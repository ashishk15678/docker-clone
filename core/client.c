#include "client.h"

int connect_to_daemon(const char* host, int port) {
    int socket_fd;
    struct sockaddr_in server_addr;

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        return -1;
    }

    // Connect to daemon
    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int send_request_to_daemon(int socket, const char* method, const char* url, const char* body) {
    char request[MAX_REQUEST_SIZE];
    int bytes_sent;

    if (create_http_request(request, method, url, body) != 0) {
        return -1;
    }

    bytes_sent = send(socket, request, strlen(request), 0);
    if (bytes_sent < 0) {
        perror("send");
        return -1;
    }

    return 0;
}

int receive_response_from_daemon(int socket, char* response, int max_size) {
    int bytes_received;

    bytes_received = recv(socket, response, max_size - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        return -1;
    }

    response[bytes_received] = '\0';
    return bytes_received;
}

int create_http_request(char* request, const char* method, const char* url, const char* body) {
    int body_length = body ? strlen(body) : 0;

    snprintf(request, MAX_REQUEST_SIZE,
             "%s %s HTTP/1.1\r\n"
             "Host: localhost\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             method, url, body_length, body ? body : "");

    return 0;
}

int parse_http_response(const char* response, int* status_code, char* body) {
    char *status_line, *body_start;

    // Find status line
    status_line = strstr(response, "HTTP/1.1 ");
    if (!status_line) {
        return -1;
    }

    // Extract status code
    sscanf(status_line, "HTTP/1.1 %d", status_code);

    // Find body (after double CRLF)
    body_start = strstr(response, "\r\n\r\n");
    if (!body_start) {
        return -1;
    }

    body_start += 4; // Skip "\r\n\r\n"
    strcpy(body, body_start);

    return 0;
}

int docker_run(const char* image, const char* command, const char* name,
               const char* working_dir, const char* env_vars,
               const char* port_mappings, const char* volume_mappings,
               int interactive, int tty, int detach) {
    int socket_fd;
    char request_body[1024];
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Create request body
    snprintf(request_body, sizeof(request_body),
             "{\"Image\":\"%s\",\"Cmd\":[\"%s\"],\"WorkingDir\":\"%s\",\"Env\":[\"%s\"],\"PortBindings\":\"%s\",\"Binds\":[\"%s\"],\"AttachStdin\":%s,\"AttachStdout\":%s,\"Detach\":%s}",
             image ? image : "ubuntu",
             command ? command : "",
             working_dir ? working_dir : "/",
             env_vars ? env_vars : "",
             port_mappings ? port_mappings : "",
             volume_mappings ? volume_mappings : "",
             interactive ? "true" : "false",
             tty ? "true" : "false",
             detach ? "true" : "false");

    // Send request
    if (send_request_to_daemon(socket_fd, "POST", "/containers/create", request_body) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 201) {
        printf("Container created successfully\n");
        return 0;
    } else {
        fprintf(stderr, "Failed to create container: %s\n", response_body);
        return -1;
    }
}

int docker_build(const char* image_name, const char* dockerfile_path, const char* context_path) {
    int socket_fd;
    char url[512];
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Create URL with query parameters
    snprintf(url, sizeof(url), "/build?t=%s&dockerfile=%s",
             image_name ? image_name : "myimage",
             dockerfile_path ? dockerfile_path : "Dockerfile");

    // Send request
    if (send_request_to_daemon(socket_fd, "POST", url, NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        printf("Image built successfully\n");
        return 0;
    } else {
        fprintf(stderr, "Failed to build image: %s\n", response_body);
        return -1;
    }
}

int docker_images() {
    int socket_fd;
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Send request
    if (send_request_to_daemon(socket_fd, "GET", "/images/json", NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        print_images_json(response_body);
        return 0;
    } else {
        fprintf(stderr, "Failed to list images: %s\n", response_body);
        return -1;
    }
}

int docker_containers() {
    int socket_fd;
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Send request
    if (send_request_to_daemon(socket_fd, "GET", "/containers/json", NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        print_containers_json(response_body);
        return 0;
    } else {
        fprintf(stderr, "Failed to list containers: %s\n", response_body);
        return -1;
    }
}

int docker_ps() {
    return docker_containers();
}

int docker_stop(const char* container_id) {
    int socket_fd;
    char url[512];
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    if (!container_id) {
        fprintf(stderr, "Container ID required\n");
        return -1;
    }

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Create URL
    snprintf(url, sizeof(url), "/containers/%s/stop", container_id);

    // Send request
    if (send_request_to_daemon(socket_fd, "POST", url, NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 204) {
        printf("Container %s stopped\n", container_id);
        return 0;
    } else {
        fprintf(stderr, "Failed to stop container: %s\n", response_body);
        return -1;
    }
}

int docker_rm(const char* container_id) {
    int socket_fd;
    char url[512];
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    if (!container_id) {
        fprintf(stderr, "Container ID required\n");
        return -1;
    }

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Create URL
    snprintf(url, sizeof(url), "/containers/%s/remove", container_id);

    // Send request
    if (send_request_to_daemon(socket_fd, "DELETE", url, NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 204) {
        printf("Container %s removed\n", container_id);
        return 0;
    } else {
        fprintf(stderr, "Failed to remove container: %s\n", response_body);
        return -1;
    }
}

int docker_rmi(const char* image_name) {
    int socket_fd;
    char url[512];
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    if (!image_name) {
        fprintf(stderr, "Image name required\n");
        return -1;
    }

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Create URL
    snprintf(url, sizeof(url), "/images/%s", image_name);

    // Send request
    if (send_request_to_daemon(socket_fd, "DELETE", url, NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        printf("Image %s removed\n", image_name);
        return 0;
    } else {
        fprintf(stderr, "Failed to remove image: %s\n", response_body);
        return -1;
    }
}

int docker_logs(const char* container_id) {
    if (!container_id) {
        fprintf(stderr, "Container ID required\n");
        return -1;
    }

    printf("Logs for container %s:\n", container_id);
    printf("(Logs functionality not yet implemented)\n");
    return 0;
}

int docker_exec(const char* container_id, const char* command) {
    if (!container_id || !command) {
        fprintf(stderr, "Container ID and command required\n");
        return -1;
    }

    printf("Executing '%s' in container %s\n", command, container_id);
    printf("(Exec functionality not yet implemented)\n");
    return 0;
}

int docker_commit(const char* container_id, const char* image_name, const char* message) {
    if (!container_id || !image_name) {
        fprintf(stderr, "Container ID and image name required\n");
        return -1;
    }

    printf("Committing container %s to image %s\n", container_id, image_name);
    if (message) {
        printf("Message: %s\n", message);
    }
    printf("(Commit functionality not yet implemented)\n");
    return 0;
}

int docker_version() {
    int socket_fd;
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Send request
    if (send_request_to_daemon(socket_fd, "GET", "/version", NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        printf("Docker version information:\n%s\n", response_body);
        return 0;
    } else {
        fprintf(stderr, "Failed to get version: %s\n", response_body);
        return -1;
    }
}

int docker_info() {
    int socket_fd;
    char response[MAX_RESPONSE_SIZE];
    int status_code;
    char response_body[MAX_RESPONSE_SIZE];

    // Connect to daemon
    socket_fd = connect_to_daemon(DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to daemon\n");
        return -1;
    }

    // Send request
    if (send_request_to_daemon(socket_fd, "GET", "/info", NULL) != 0) {
        close(socket_fd);
        return -1;
    }

    // Receive response
    if (receive_response_from_daemon(socket_fd, response, sizeof(response)) < 0) {
        close(socket_fd);
        return -1;
    }

    // Parse response
    if (parse_http_response(response, &status_code, response_body) != 0) {
        close(socket_fd);
        return -1;
    }

    close(socket_fd);

    if (status_code == 200) {
        printf("Docker system information:\n%s\n", response_body);
        return 0;
    } else {
        fprintf(stderr, "Failed to get info: %s\n", response_body);
        return -1;
    }
}

void print_containers_json(const char* json) {
    printf("CONTAINER ID    IMAGE    COMMAND    CREATED    STATUS\n");
    printf("----------------------------------------------------\n");

    // Simple JSON parsing - in a real implementation, use a proper JSON library
    char *id_start, *name_start, *image_start, *command_start, *created_start, *status_start;

    id_start = strstr(json, "\"Id\":\"");
    if (id_start) {
        id_start += 6;
        char *id_end = strchr(id_start, '"');
        if (id_end) {
            printf("%.*s    ", (int)(id_end - id_start), id_start);
        }
    }

    name_start = strstr(json, "\"Names\":[\"");
    if (name_start) {
        name_start += 11;
        char *name_end = strchr(name_start, '"');
        if (name_end) {
            printf("%.*s    ", (int)(name_end - name_start), name_start);
        }
    }

    image_start = strstr(json, "\"Image\":\"");
    if (image_start) {
        image_start += 9;
        char *image_end = strchr(image_start, '"');
        if (image_end) {
            printf("%.*s    ", (int)(image_end - image_start), image_start);
        }
    }

    command_start = strstr(json, "\"Command\":\"");
    if (command_start) {
        command_start += 11;
        char *command_end = strchr(command_start, '"');
        if (command_end) {
            printf("%.*s    ", (int)(command_end - command_start), command_start);
        }
    }

    created_start = strstr(json, "\"Created\":");
    if (created_start) {
        created_start += 10;
        char *created_end = strchr(created_start, ',');
        if (!created_end) created_end = strchr(created_start, '}');
        if (created_end) {
            printf("%.*s    ", (int)(created_end - created_start), created_start);
        }
    }

    status_start = strstr(json, "\"Status\":\"");
    if (status_start) {
        status_start += 10;
        char *status_end = strchr(status_start, '"');
        if (status_end) {
            printf("%.*s\n", (int)(status_end - status_start), status_start);
        }
    }
}

void print_images_json(const char* json) {
    printf("REPOSITORY    TAG    IMAGE ID    CREATED    SIZE\n");
    printf("------------------------------------------------\n");

    // Simple JSON parsing - in a real implementation, use a proper JSON library
    char *repo_start, *tag_start, *id_start, *created_start, *size_start;

    repo_start = strstr(json, "\"RepoTags\":[\"");
    if (repo_start) {
        repo_start += 13;
        char *repo_end = strchr(repo_start, ':');
        if (repo_end) {
            printf("%.*s    ", (int)(repo_end - repo_start), repo_start);
        }
    }

    tag_start = strstr(json, "\"RepoTags\":[\"");
    if (tag_start) {
        tag_start += 13;
        char *colon = strchr(tag_start, ':');
        if (colon) {
            colon++;
            char *tag_end = strchr(colon, '"');
            if (tag_end) {
                printf("%.*s    ", (int)(tag_end - colon), colon);
            }
        }
    }

    id_start = strstr(json, "\"Id\":\"");
    if (id_start) {
        id_start += 6;
        char *id_end = strchr(id_start, '"');
        if (id_end) {
            printf("%.*s    ", (int)(id_end - id_start), id_start);
        }
    }

    created_start = strstr(json, "\"Created\":");
    if (created_start) {
        created_start += 10;
        char *created_end = strchr(created_start, ',');
        if (!created_end) created_end = strchr(created_start, '}');
        if (created_end) {
            printf("%.*s    ", (int)(created_end - created_start), created_start);
        }
    }

    size_start = strstr(json, "\"Size\":");
    if (size_start) {
        size_start += 7;
        char *size_end = strchr(size_start, ',');
        if (!size_end) size_end = strchr(size_start, '}');
        if (size_end) {
            printf("%.*s\n", (int)(size_end - size_start), size_start);
        }
    }
}

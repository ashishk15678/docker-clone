#include "http.h"
#include "container.h"
#include "image.h"
#include "dockerfile.h"

int start_http_server(int port) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;
    client_info_t *client_info;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return -1;
    }

    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        return -1;
    }

    // Listen for connections
    if (listen(server_socket, MAX_THREADS) < 0) {
        perror("listen");
        close(server_socket);
        return -1;
    }

    printf("Docker daemon listening on port %d\n", port);

    // Accept connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        // Create client info structure
        client_info = malloc(sizeof(client_info_t));
        if (!client_info) {
            perror("malloc");
            close(client_socket);
            continue;
        }

        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;

        // Create thread to handle client
        if (pthread_create(&thread_id, NULL, handle_client, client_info) != 0) {
            perror("pthread_create");
            free(client_info);
            close(client_socket);
            continue;
        }

        // Detach thread
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}

void* handle_client(void* arg) {
    client_info_t *client_info = (client_info_t*)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_addr = client_info->client_addr;
    char request_buffer[MAX_REQUEST_SIZE];
    http_request_t request;
    http_response_t response;
    int bytes_received;

    // Receive request
    bytes_received = recv(client_socket, request_buffer, sizeof(request_buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        free(client_info);
        return NULL;
    }

    request_buffer[bytes_received] = '\0';

    // Parse request
    if (parse_http_request(request_buffer, &request) != 0) {
        create_http_response(&response, 400, "Bad Request", "Invalid HTTP request");
        send_http_response(client_socket, &response);
        close(client_socket);
        free(client_info);
        return NULL;
    }

    // Log request
    log_request(&request, &client_addr);

    // Handle API request
    if (handle_api_request(&request, &response) != 0) {
        create_http_response(&response, 500, "Internal Server Error", "Failed to handle request");
    }

    // Log response
    log_response(&response);

    // Send response
    send_http_response(client_socket, &response);

    close(client_socket);
    free(client_info);
    return NULL;
}

int parse_http_request(const char* request, http_request_t* parsed) {
    char *line, *method, *url, *version;
    char *request_copy = strdup(request);

    if (!request_copy) {
        return -1;
    }

    // Parse first line (method, URL, version)
    line = strtok(request_copy, "\r\n");
    if (!line) {
        free(request_copy);
        return -1;
    }

    method = strtok(line, " ");
    url = strtok(NULL, " ");
    version = strtok(NULL, " ");

    if (!method || !url || !version) {
        free(request_copy);
        return -1;
    }

    strncpy(parsed->method, method, sizeof(parsed->method) - 1);
    strncpy(parsed->url, url, sizeof(parsed->url) - 1);
    strncpy(parsed->version, version, sizeof(parsed->version) - 1);

    // Parse headers
    char *header_line;
    parsed->headers[0] = '\0';

    while ((header_line = strtok(NULL, "\r\n")) != NULL) {
        if (strlen(header_line) == 0) {
            break; // End of headers
        }

        if (strstr(header_line, "Content-Length:")) {
            sscanf(header_line, "Content-Length: %d", &parsed->content_length);
        }

        strcat(parsed->headers, header_line);
        strcat(parsed->headers, "\r\n");
    }

    // Parse body
    char *body = strtok(NULL, "");
    if (body) {
        strncpy(parsed->body, body, sizeof(parsed->body) - 1);
    } else {
        parsed->body[0] = '\0';
    }

    free(request_copy);
    return 0;
}

int create_http_response(http_response_t* response, int status_code, const char* status_message, const char* body) {
    strcpy(response->version, "HTTP/1.1");
    response->status_code = status_code;
    strncpy(response->status_message, status_message, sizeof(response->status_message) - 1);

    if (body) {
        strncpy(response->body, body, sizeof(response->body) - 1);
        response->content_length = strlen(body);
    } else {
        response->body[0] = '\0';
        response->content_length = 0;
    }

    // Set headers
    snprintf(response->headers, sizeof(response->headers),
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n",
             response->content_length);

    return 0;
}

int send_http_response(int client_socket, http_response_t* response) {
    char response_buffer[MAX_RESPONSE_SIZE];
    int bytes_sent;

    // Build response
    snprintf(response_buffer, sizeof(response_buffer),
             "%s %d %s\r\n"
             "%s"
             "\r\n"
             "%s",
             response->version,
             response->status_code,
             response->status_message,
             response->headers,
             response->body);

    // Send response
    bytes_sent = send(client_socket, response_buffer, strlen(response_buffer), 0);
    if (bytes_sent < 0) {
        perror("send");
        return -1;
    }

    return 0;
}

int handle_api_request(http_request_t* request, http_response_t* response) {
    // Route requests based on URL
    if (strstr(request->url, "/containers")) {
        return handle_containers_api(request, response);
    } else if (strstr(request->url, "/images")) {
        return handle_images_api(request, response);
    // } else if (strstr(request->url, "/version")) {
    //     return handle_version_api(request, response);
    // } else if (strstr(request->url, "/info")) {
    //     return handle_info_api(request, response);
    } else {
        create_http_response(response, 404, "Not Found", "{\"error\": \"API endpoint not found\"}");
        return 0;
    }
}

int handle_containers_api(http_request_t* request, http_response_t* response) {
    if (strcmp(request->method, "GET") == 0) {
        if (strstr(request->url, "/containers/json")) {
            // return handle_container_list(request, response);
        } else if (strstr(request->url, "/containers/") && strstr(request->url, "/start")) {
            return handle_container_start(request, response);
        } else if (strstr(request->url, "/containers/") && strstr(request->url, "/stop")) {
            return handle_container_stop(request, response);
        } else if (strstr(request->url, "/containers/") && strstr(request->url, "/remove")) {
            return handle_container_remove(request, response);
        }
    } else if (strcmp(request->method, "POST") == 0) {
        if (strstr(request->url, "/containers/create")) {
            return handle_container_create(request, response);
        }
    }

    create_http_response(response, 404, "Not Found", "{\"error\": \"Container API endpoint not found\"}");
    return 0;
}

int handle_images_api(http_request_t* request, http_response_t* response) {
    if (strcmp(request->method, "GET") == 0) {
        if (strstr(request->url, "/images/json")) {
            return handle_image_list(request, response);
        }
    } else if (strcmp(request->method, "POST") == 0) {
        if (strstr(request->url, "/build")) {
            return handle_image_build(request, response);
        }
    } else if (strcmp(request->method, "DELETE") == 0) {
        if (strstr(request->url, "/images/")) {
            return handle_image_remove(request, response);
        }
    }

    create_http_response(response, 404, "Not Found", "{\"error\": \"Image API endpoint not found\"}");
    return 0;
}

int handle_container_create(http_request_t* request, http_response_t* response) {
    char container_name[256] = {0};
    char image_name[256] = {0};
    char command[512] = {0};
    char working_dir[256] = {0};
    char env_vars[512] = {0};
    char port_mappings[256] = {0};
    char volume_mappings[256] = {0};
    int interactive = 0, tty = 0, detach = 0;

    // Parse JSON body (simplified)
    if (strstr(request->body, "\"Image\"")) {
        sscanf(request->body, "\"Image\":\"%[^\"]\"", image_name);
    }
    if (strstr(request->body, "\"Cmd\"")) {
        sscanf(request->body, "\"Cmd\":[\"%[^\"]\"", command);
    }
    if (strstr(request->body, "\"WorkingDir\"")) {
        sscanf(request->body, "\"WorkingDir\":\"%[^\"]\"", working_dir);
    }
    if (strstr(request->body, "\"Env\"")) {
        sscanf(request->body, "\"Env\":[\"%[^\"]\"", env_vars);
    }
    if (strstr(request->body, "\"PortBindings\"")) {
        sscanf(request->body, "\"PortBindings\":\"%[^\"]\"", port_mappings);
    }
    if (strstr(request->body, "\"Binds\"")) {
        sscanf(request->body, "\"Binds\":[\"%[^\"]\"", volume_mappings);
    }
    if (strstr(request->body, "\"AttachStdin\"")) {
        interactive = 1;
    }
    if (strstr(request->body, "\"AttachStdout\"")) {
        tty = 1;
    }
    if (strstr(request->body, "\"Detach\"")) {
        detach = 1;
    }

    if (strlen(image_name) == 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Image name required\"}");
        return 0;
    }

    int result = create_container(container_name, image_name, command, working_dir,
                                 env_vars, port_mappings, volume_mappings,
                                 interactive, tty, detach);

    if (result == 0) {
        create_http_response(response, 201, "Created", "{\"Id\": \"container_created\", \"Warnings\": []}");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to create container\"}");
    }

    return 0;
}

int handle_container_start(http_request_t* request, http_response_t* response) {
    char container_id[256];

    if (extract_container_id_from_url(request->url, container_id) != 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Invalid container ID\"}");
        return 0;
    }

    int result = start_container(container_id);

    if (result == 0) {
        create_http_response(response, 204, "No Content", "");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to start container\"}");
    }

    return 0;
}

int handle_container_stop(http_request_t* request, http_response_t* response) {
    char container_id[256];

    if (extract_container_id_from_url(request->url, container_id) != 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Invalid container ID\"}");
        return 0;
    }

    int result = stop_container(container_id);

    if (result == 0) {
        create_http_response(response, 204, "No Content", "");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to stop container\"}");
    }

    return 0;
}

int handle_container_remove(http_request_t* request, http_response_t* response) {
    char container_id[256];

    if (extract_container_id_from_url(request->url, container_id) != 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Invalid container ID\"}");
        return 0;
    }

    int result = remove_container(container_id);

    if (result == 0) {
        create_http_response(response, 204, "No Content", "");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to remove container\"}");
    }

    return 0;
}

int handle_container_list(http_request_t* request, http_response_t* response) {
    container_list_t *containers = list_containers();

    if (!containers) {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to list containers\"}");
        return 0;
    }

    char json_response[4096] = "[";
    char container_json[512];

    for (int i = 0; i < containers->count; i++) {
        snprintf(container_json, sizeof(container_json),
                 "{\"Id\":\"%s\",\"Names\":[\"%s\"],\"Image\":\"%s\",\"Command\":\"%s\",\"Created\":%s,\"Status\":\"%s\"}",
                 containers->containers[i].id,
                 containers->containers[i].name,
                 containers->containers[i].image,
                 containers->containers[i].command,
                 containers->containers[i].created,
                 containers->containers[i].state == CONTAINER_STATE_RUNNING ? "running" : "exited");

        strcat(json_response, container_json);
        if (i < containers->count - 1) {
            strcat(json_response, ",");
        }
    }

    strcat(json_response, "]");

    create_http_response(response, 200, "OK", json_response);

    // Free containers list
    free(containers->containers);
    free(containers);

    return 0;
}

int handle_image_build(http_request_t* request, http_response_t* response) {
    char image_name[256] = {0};
    char dockerfile_path[256] = {0};
    char context_path[256] = {0};

    // Parse query parameters
    if (strstr(request->url, "t=")) {
        sscanf(strstr(request->url, "t="), "t=%[^&]", image_name);
    }
    if (strstr(request->url, "dockerfile=")) {
        sscanf(strstr(request->url, "dockerfile="), "dockerfile=%[^&]", dockerfile_path);
    }

    if (strlen(image_name) == 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Image name required\"}");
        return 0;
    }

    if (strlen(dockerfile_path) == 0) {
        strcpy(dockerfile_path, "Dockerfile");
    }

    strcpy(context_path, ".");

    // Parse Dockerfile
    dockerfile_t *dockerfile = parse_dockerfile(dockerfile_path);
    if (!dockerfile) {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to parse Dockerfile\"}");
        return 0;
    }

    // Build image
    int result = build_image_from_dockerfile(dockerfile, image_name, "latest", context_path);

    free_dockerfile(dockerfile);

    if (result == 0) {
        create_http_response(response, 200, "OK", "{\"message\": \"Image built successfully\"}");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to build image\"}");
    }

    return 0;
}

int handle_image_list(http_request_t* request, http_response_t* response) {
    image_list_t *images = list_images();

    if (!images) {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to list images\"}");
        return 0;
    }

    char json_response[4096] = "[";
    char image_json[512];

    for (int i = 0; i < images->count; i++) {
        snprintf(image_json, sizeof(image_json),
                 "{\"Id\":\"%s\",\"RepoTags\":[\"%s:%s\"],\"Created\":%s,\"Size\":%s}",
                 images->images[i].id,
                 images->images[i].name,
                 images->images[i].tag,
                 images->images[i].created,
                 images->images[i].size);

        strcat(json_response, image_json);
        if (i < images->count - 1) {
            strcat(json_response, ",");
        }
    }

    strcat(json_response, "]");

    create_http_response(response, 200, "OK", json_response);

    // Free images list
    free(images->images);
    free(images);

    return 0;
}

int handle_image_remove(http_request_t* request, http_response_t* response) {
    char image_name[256];

    if (extract_image_name_from_url(request->url, image_name) != 0) {
        create_http_response(response, 400, "Bad Request", "{\"error\": \"Invalid image name\"}");
        return 0;
    }

    int result = remove_image(image_name);

    if (result == 0) {
        create_http_response(response, 200, "OK", "{\"message\": \"Image removed successfully\"}");
    } else {
        create_http_response(response, 500, "Internal Server Error", "{\"error\": \"Failed to remove image\"}");
    }

    return 0;
}

int handle_version_api(http_request_t* request, http_response_t* response) {
    char version_json[] = "{\"Version\":\"1.0.0\",\"ApiVersion\":\"1.40\",\"GitCommit\":\"docker-clone\",\"GoVersion\":\"N/A\",\"Os\":\"linux\",\"Arch\":\"amd64\"}";
    create_http_response(response, 200, "OK", version_json);
    return 0;
}

int handle_info_api(http_request_t* request, http_response_t* response) {
    char info_json[] = "{\"Containers\":0,\"Images\":0,\"Driver\":\"overlay2\",\"SystemTime\":\"2024-01-01T00:00:00Z\"}";
    create_http_response(response, 200, "OK", info_json);
    return 0;
}

int extract_container_id_from_url(const char* url, char* container_id) {
    const char *start = strstr(url, "/containers/");
    if (!start) {
        return -1;
    }

    start += strlen("/containers/");
    const char *end = strchr(start, '/');
    if (!end) {
        end = start + strlen(start);
    }

    int len = end - start;
    if (len >= 256) {
        return -1;
    }

    strncpy(container_id, start, len);
    container_id[len] = '\0';

    return 0;
}

int extract_image_name_from_url(const char* url, char* image_name) {
    const char *start = strstr(url, "/images/");
    if (!start) {
        return -1;
    }

    start += strlen("/images/");
    const char *end = strchr(start, '/');
    if (!end) {
        end = start + strlen(start);
    }

    int len = end - start;
    if (len >= 256) {
        return -1;
    }

    strncpy(image_name, start, len);
    image_name[len] = '\0';

    return 0;
}

void log_request(http_request_t* request, struct sockaddr_in* client_addr) {
    printf("[%s] %s %s %s from %s\n",
           request->version, request->method, request->url, request->version,
           inet_ntoa(client_addr->sin_addr));
}

void log_response(http_response_t* response) {
    printf("Response: %s %d %s\n",
           response->version, response->status_code, response->status_message);
}

void cleanup_server(int server_socket) {
    if (server_socket >= 0) {
        close(server_socket);
    }
}

#include <cstdlib>
#include <functional>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void *createServer(int port) {
  struct sockaddr_in address;
  int address_len = sizeof(address);

  int server_fd, new_socket;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed ");
    exit(127);
  }

  address.sin_family = AF_INET;
  address.sin_port = port;
  address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(127);
  }

  if (listen(server_fd, 3) < 0) {
    perror("error");
    exit(EXIT_FAILURE);
  }

  printf("serrver listening on %d", port);
}

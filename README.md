# Docker Clone

A Docker-like containerization system implemented in pure C without external dependencies.

## Features

- **Container Management**: Create, start, stop, and remove containers
- **Image System**: Build images from Dockerfiles with layer support
- **Dockerfile Parser**: Parse and execute Dockerfile instructions
- **HTTP API**: RESTful API for daemon communication
- **Namespace Isolation**: PID, UTS, mount, and network namespaces
- **Cgroup Support**: Resource limits and process isolation
- **Client-Daemon Architecture**: Separate client and daemon processes

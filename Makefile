CC        = cc
CFLAGS    = -Wall -Wextra -std=c11 -g -D_GNU_SOURCE
LDFLAGS   = -lpthread

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj

CLIENT_NAME  = docker-clone
DAEMON_NAME  = docker-clone-daemon

CLIENT_TARGET = $(BUILD_DIR)/$(CLIENT_NAME)
DAEMON_TARGET = $(BUILD_DIR)/$(DAEMON_NAME)

CLIENT_SRCS = \
	main.c \
	core/cli-parser.c \
	core/client.c \
	core/daemon.c

DAEMON_SRCS = \
	daemon.c \
	core/daemon.c \
	core/daemon_runtime.c \
	core/http.c \
	core/container.c \
	core/image.c \
	core/dockerfile.c

CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(OBJ_DIR)/%.o)
DAEMON_OBJS = $(DAEMON_SRCS:%.c=$(OBJ_DIR)/%.o)

CLIENT_RUN_SCRIPT = run-client.sh
DAEMON_RUN_SCRIPT = run-daemon.sh

CLIENT_BIN := $(abspath $(CLIENT_TARGET))
DAEMON_BIN := $(abspath $(DAEMON_TARGET))

.PHONY: all client daemon main run-client run-daemon install clean help

all: $(CLIENT_TARGET) $(DAEMON_TARGET) $(CLIENT_RUN_SCRIPT) $(DAEMON_RUN_SCRIPT)

client main: $(CLIENT_TARGET)
daemon: $(DAEMON_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS) | $(BUILD_DIR)
	@echo "Linking client: $(CLIENT_NAME)"
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(DAEMON_TARGET): $(DAEMON_OBJS) | $(BUILD_DIR)
	@echo "Linking daemon: $(DAEMON_NAME)"
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR): | $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)

$(CLIENT_RUN_SCRIPT): $(CLIENT_TARGET)
	@echo "Generating client run script: $@"
	@echo "#!/bin/bash" > $@
	@echo "\"$(CLIENT_BIN)\" \"\$$@\"" >> $@
	@chmod +x $@

$(DAEMON_RUN_SCRIPT): $(DAEMON_TARGET)
	@echo "Generating daemon run script: $@"
	@echo "#!/bin/bash" > $@
	@echo "\"$(DAEMON_BIN)\" \"\$$@\"" >> $@
	@chmod +x $@

run-client: $(CLIENT_TARGET)
	$(CLIENT_TARGET) $(ARGS)

run-daemon: $(DAEMON_TARGET)
	$(DAEMON_TARGET) $(ARGS)

install: all
	@echo "Installing docker-clone binaries..."
	sudo cp $(CLIENT_TARGET) /usr/local/bin/$(CLIENT_NAME)
	sudo cp $(DAEMON_TARGET) /usr/local/bin/$(DAEMON_NAME)
	@echo "Installation complete."

clean:
	@echo "Cleaning project..."
	rm -rf $(BUILD_DIR) $(CLIENT_RUN_SCRIPT) $(DAEMON_RUN_SCRIPT)
	@echo "Clean complete."

help:
	@echo "Available targets:"
	@echo "  all          - Build client and daemon binaries"
	@echo "  client/main  - Build only the CLI client"
	@echo "  daemon       - Build only the daemon"
	@echo "  run-client   - Run the client with ARGS='...'"
	@echo "  run-daemon   - Run the daemon"
	@echo "  install      - Install both binaries system-wide"
	@echo "  clean        - Remove build artifacts and run scripts"
	@echo "  help         - Show this help message"

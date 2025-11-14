# --- Configuration Variables ---
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -D_GNU_SOURCE
LDFLAGS = -lpthread

# Directory where all output (objects and binaries) will be placed
BUILD_DIR = build

# The name of the final executable binaries
CLIENT_NAME = docker-clone
DAEMON_NAME = docker-clone-daemon

# The full path to the target executables
CLIENT_TARGET = $(BUILD_DIR)/$(CLIENT_NAME)
DAEMON_TARGET = $(BUILD_DIR)/$(DAEMON_NAME)

# Source files for client
CLIENT_SRC = main.c core/cli-parser.c core/client.c core/daemon.c core/http.c core/container.c core/image.c core/dockerfile.c

# Source files for daemon
DAEMON_SRC = core/daemon.c core/http.c core/container.c core/image.c core/dockerfile.c

# Object file names without path
CLIENT_OBJ_NAMES = $(CLIENT_SRC:.c=.o)
DAEMON_OBJ_NAMES = $(DAEMON_SRC:.c=.o)

# Object files with the full path
CLIENT_OBJ = $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJ_NAMES))
DAEMON_OBJ = $(addprefix $(BUILD_DIR)/, $(DAEMON_OBJ_NAMES))

# Name of the executable scripts
CLIENT_RUN_SCRIPT = run-client.sh
DAEMON_RUN_SCRIPT = run-daemon.sh

# --- Main Targets: Build the Executables ---
$(CLIENT_TARGET): $(BUILD_DIR) $(CLIENT_OBJ)
	@echo "Linking client: $(CLIENT_NAME)..."
	$(CC) $(CFLAGS) $(CLIENT_OBJ) -o $@ $(LDFLAGS)

# Default target - build the client
main: $(CLIENT_TARGET)
	@echo "Client built successfully"


$(DAEMON_TARGET): $(BUILD_DIR) $(DAEMON_OBJ)
	@echo "Linking daemon: $(DAEMON_NAME)..."
	$(CC) $(CFLAGS) $(DAEMON_OBJ) -o $@ $(LDFLAGS)

# --- Rule to Compile .c to .o (Object Files) ---
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

# --- Utility Targets ---
.PHONY: all clean run-client run-daemon setup install

# The 'all' target builds both client and daemon
all: $(CLIENT_TARGET) $(DAEMON_TARGET) $(CLIENT_RUN_SCRIPT) $(DAEMON_RUN_SCRIPT)

# Rule to create the build directory if it doesn't exist
$(BUILD_DIR):
	@echo "Creating output directory: $(BUILD_DIR)"
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/core

# Rule to generate the run scripts
$(CLIENT_RUN_SCRIPT): $(CLIENT_TARGET)
	@echo "Generating client run script: $@"
	@echo "#!/bin/bash" > $@
	@echo "echo 'Running $(CLIENT_NAME) from $(BUILD_DIR)...'" >> $@
	@echo "$< \"\$$@\"" >> $@
	@chmod +x $@

$(DAEMON_RUN_SCRIPT): $(DAEMON_TARGET)
	@echo "Generating daemon run script: $@"
	@echo "#!/bin/bash" > $@
	@echo "echo 'Running $(DAEMON_NAME) from $(BUILD_DIR)...'" >> $@
	@echo "$<" >> $@
	@chmod +x $@

# Run targets
run-client: $(CLIENT_TARGET)
	@echo "Running client..."
	$(CLIENT_TARGET) $(ARGS)

run-daemon: $(DAEMON_TARGET)
	@echo "Running daemon..."
	$(DAEMON_TARGET)

# Install targets
install: all
	@echo "Installing docker-clone..."
	sudo cp $(CLIENT_TARGET) /usr/local/bin/$(CLIENT_NAME)
	sudo cp $(DAEMON_TARGET) /usr/local/bin/$(DAEMON_NAME)
	@echo "Installation complete."

# Use 'make clean' to remove the entire build directory and the run scripts
clean:
	@echo "Cleaning project..."
	rm -rf $(BUILD_DIR) $(CLIENT_RUN_SCRIPT) $(DAEMON_RUN_SCRIPT)
	@echo "Clean complete."

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both client and daemon"
	@echo "  client       - Build only the client"
	@echo "  daemon       - Build only the daemon"
	@echo "  run-client   - Run the client with ARGS='...'"
	@echo "  run-daemon   - Run the daemon"
	@echo "  install      - Install binaries to /usr/local/bin"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help message"

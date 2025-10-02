# --- Configuration Variables ---
# --- Configuration Variables ---
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Directory where all output (objects and binaries) will be placed
BUILD_DIR = build

# The name of the final executable binary
TARGET_NAME = my_program

# The full path to the target executable
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

# Source files
SRC = main.c daemon.c

# Object file names without path (e.g., main.o)
OBJ_NAMES = $(SRC:.c=)

# Object files with the full path (e.g., build/main.o)
OBJ = $(addprefix $(BUILD_DIR)/, $(OBJ_NAMES))

# Name of the executable script
RUN_SCRIPT = run.sh

# --- Main Target: Build the Executable (The Runnable Binary) ---
# Dependencies: The build directory must exist, and all object files must be compiled.
$(TARGET): $(BUILD_DIR) $(OBJ)
	@echo "Linking target: $(TARGET_NAME)..."
	# The link command takes all compiled objects and places the final binary in the build directory
	$(CC) $(CFLAGS) $(OBJ) -o $@

# --- Rule to Compile .c to .o (Object Files) ---
# The pattern matching rule now ensures the output (.o) goes into the build directory.
# The | $(BUILD_DIR) is an "order-only" prerequisite, ensuring the directory is created first.
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

# --- Utility Targets ---
.PHONY: all clean run setup

# The 'all' target now also builds the run script
all: $(TARGET) $(RUN_SCRIPT)

# Rule to create the build directory if it doesn't exist
$(BUILD_DIR):
	@echo "Creating output directory: $(BUILD_DIR)"
	mkdir -p $(BUILD_DIR)

# Rule to generate the run.sh script
$(RUN_SCRIPT): $(TARGET)
	@echo "Generating run script: $@"
	@echo "#!/bin/bash" > $@
	@echo "echo 'Running $(TARGET_NAME) from $(BUILD_DIR)...'" >> $@
	@echo "$<" >> $@
	@chmod +x $@

# Use 'make clean' to remove the entire build directory and the run.sh script
clean:
	@echo "Cleaning project..."
	rm -rf $(BUILD_DIR) $(RUN_SCRIPT)
	@echo "Clean complete."

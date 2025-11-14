#ifndef IMAGE_H
#define IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MAX_IMAGE_NAME_LEN 256
#define MAX_IMAGE_TAG_LEN 64
#define MAX_IMAGE_ID_LEN 64
#define MAX_LAYER_ID_LEN 64
#define MAX_PATH_LEN 512
#define MAX_COMMAND_LEN 1024
#define MAX_ENV_VAR_LEN 512
#define MAX_WORKDIR_LEN 256
#define MAX_EXPOSE_LEN 64
#define MAX_VOLUME_LEN 256

#define IMAGE_STORAGE_DIR "/tmp/docker-images"
#define LAYER_STORAGE_DIR "/tmp/docker-layers"
#define METADATA_DIR "/tmp/docker-metadata"

typedef struct {
    char id[MAX_LAYER_ID_LEN];
    char parent_id[MAX_LAYER_ID_LEN];
    char command[MAX_COMMAND_LEN];
    char created[32];
    char size[32];
    char diff_id[MAX_LAYER_ID_LEN];
} layer_info_t;

typedef struct {
    char id[MAX_IMAGE_ID_LEN];
    char name[MAX_IMAGE_NAME_LEN];
    char tag[MAX_IMAGE_TAG_LEN];
    char parent_id[MAX_IMAGE_ID_LEN];
    char created[32];
    char size[32];
    char architecture[32];
    char os[32];
    char author[128];
    char comment[256];
    char command[MAX_COMMAND_LEN];
    char working_dir[MAX_WORKDIR_LEN];
    char env_vars[MAX_ENV_VAR_LEN];
    char exposed_ports[MAX_EXPOSE_LEN];
    char volumes[MAX_VOLUME_LEN];
    layer_info_t *layers;
    int layer_count;
} image_info_t;

typedef struct {
    image_info_t *images;
    int count;
    int capacity;
} image_list_t;

// Function declarations
int init_image_system();
int create_image(const char *name, const char *tag, const char *dockerfile_path, const char *context_path);
int load_image(const char *image_path);
int save_image(const char *image_id, const char *output_path);
int remove_image(const char *image_id);
int tag_image(const char *image_id, const char *name, const char *tag);
image_list_t* list_images();
image_info_t* get_image_info(const char *image_id);
int image_exists(const char *name, const char *tag);
char* generate_image_id();
char* generate_layer_id();
int create_layer(const char *parent_id, const char *command, const char *diff_path);
int extract_layer(const char *layer_id, const char *target_path);
int cleanup_image_system();

// Helper functions
char* get_image_full_name(const char *name, const char *tag);
int write_image_metadata(image_info_t *image);
int read_image_metadata(const char *image_id, image_info_t *image);
int create_directory_structure();
int calculate_directory_size(const char *path);

#endif // IMAGE_H


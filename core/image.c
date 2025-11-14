#include "image.h"

int init_image_system() {
    if (create_directory_structure() != 0) {
        return -1;
    }
    return 0;
}

int create_directory_structure() {
    char dirs[][MAX_PATH_LEN] = {
        IMAGE_STORAGE_DIR,
        LAYER_STORAGE_DIR,
        METADATA_DIR
    };
    
    for (int i = 0; i < 3; i++) {
        if (mkdir(dirs[i], 0755) != 0 && errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

char* generate_image_id() {
    static char id[MAX_IMAGE_ID_LEN];
    time_t now = time(NULL);
    snprintf(id, sizeof(id), "sha256:%08x%08x", (unsigned int)now, (unsigned int)(now >> 32));
    return id;
}

char* generate_layer_id() {
    static char id[MAX_LAYER_ID_LEN];
    time_t now = time(NULL);
    snprintf(id, sizeof(id), "layer_%08x%08x", (unsigned int)now, (unsigned int)(now >> 32));
    return id;
}

char* get_image_full_name(const char *name, const char *tag) {
    static char full_name[MAX_IMAGE_NAME_LEN + MAX_IMAGE_TAG_LEN + 2];
    if (tag && strlen(tag) > 0) {
        snprintf(full_name, sizeof(full_name), "%s:%s", name, tag);
    } else {
        snprintf(full_name, sizeof(full_name), "%s:latest", name);
    }
    return full_name;
}

int image_exists(const char *name, const char *tag) {
    char full_name[MAX_IMAGE_NAME_LEN + MAX_IMAGE_TAG_LEN + 2];
    char metadata_path[MAX_PATH_LEN];
    
    snprintf(full_name, sizeof(full_name), "%s", get_image_full_name(name, tag));
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", METADATA_DIR, full_name);
    
    return access(metadata_path, F_OK) == 0;
}

int create_layer(const char *parent_id, const char *command, const char *diff_path) {
    char layer_id[MAX_LAYER_ID_LEN];
    char layer_path[MAX_PATH_LEN];
    char metadata_path[MAX_PATH_LEN];
    FILE *fp;
    
    strcpy(layer_id, generate_layer_id());
    snprintf(layer_path, sizeof(layer_path), "%s/%s", LAYER_STORAGE_DIR, layer_id);
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", layer_path, layer_id);
    
    // Create layer directory
    if (mkdir(layer_path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir layer");
        return -1;
    }
    
    // Copy diff if provided
    if (diff_path && strlen(diff_path) > 0) {
        char copy_cmd[1024];
        snprintf(copy_cmd, sizeof(copy_cmd), "cp -r %s/* %s/", diff_path, layer_path);
        if (system(copy_cmd) != 0) {
            fprintf(stderr, "Failed to copy diff to layer\n");
            return -1;
        }
    }
    
    // Create layer metadata
    fp = fopen(metadata_path, "w");
    if (!fp) {
        perror("fopen layer metadata");
        return -1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"%s\",\n", layer_id);
    fprintf(fp, "  \"parent\": \"%s\",\n", parent_id ? parent_id : "");
    fprintf(fp, "  \"created\": \"%ld\",\n", time(NULL));
    fprintf(fp, "  \"container\": \"\",\n");
    fprintf(fp, "  \"container_config\": {\n");
    fprintf(fp, "    \"Cmd\": [\"%s\"]\n", command ? command : "");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"config\": {\n");
    fprintf(fp, "    \"Cmd\": [\"%s\"]\n", command ? command : "");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    return 0;
}

int extract_layer(const char *layer_id, const char *target_path) {
    char layer_path[MAX_PATH_LEN];
    char copy_cmd[1024];
    
    snprintf(layer_path, sizeof(layer_path), "%s/%s", LAYER_STORAGE_DIR, layer_id);
    
    if (access(layer_path, F_OK) != 0) {
        fprintf(stderr, "Layer %s not found\n", layer_id);
        return -1;
    }
    
    snprintf(copy_cmd, sizeof(copy_cmd), "cp -r %s/* %s/", layer_path, target_path);
    if (system(copy_cmd) != 0) {
        fprintf(stderr, "Failed to extract layer\n");
        return -1;
    }
    
    return 0;
}

int write_image_metadata(image_info_t *image) {
    char metadata_path[MAX_PATH_LEN];
    char full_name[MAX_IMAGE_NAME_LEN + MAX_IMAGE_TAG_LEN + 2];
    FILE *fp;
    
    snprintf(full_name, sizeof(full_name), "%s", get_image_full_name(image->name, image->tag));
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", METADATA_DIR, full_name);
    
    fp = fopen(metadata_path, "w");
    if (!fp) {
        perror("fopen image metadata");
        return -1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"%s\",\n", image->id);
    fprintf(fp, "  \"name\": \"%s\",\n", image->name);
    fprintf(fp, "  \"tag\": \"%s\",\n", image->tag);
    fprintf(fp, "  \"parent\": \"%s\",\n", image->parent_id);
    fprintf(fp, "  \"created\": \"%s\",\n", image->created);
    fprintf(fp, "  \"size\": \"%s\",\n", image->size);
    fprintf(fp, "  \"architecture\": \"%s\",\n", image->architecture);
    fprintf(fp, "  \"os\": \"%s\",\n", image->os);
    fprintf(fp, "  \"author\": \"%s\",\n", image->author);
    fprintf(fp, "  \"comment\": \"%s\",\n", image->comment);
    fprintf(fp, "  \"config\": {\n");
    fprintf(fp, "    \"Cmd\": [\"%s\"],\n", image->command);
    fprintf(fp, "    \"WorkingDir\": \"%s\",\n", image->working_dir);
    fprintf(fp, "    \"Env\": [\"%s\"],\n", image->env_vars);
    fprintf(fp, "    \"ExposedPorts\": {\"%s\": {}},\n", image->exposed_ports);
    fprintf(fp, "    \"Volumes\": {\"%s\": {}}\n", image->volumes);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"layers\": [\n");
    
    for (int i = 0; i < image->layer_count; i++) {
        fprintf(fp, "    \"%s\"", image->layers[i].id);
        if (i < image->layer_count - 1) fprintf(fp, ",");
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    return 0;
}

int read_image_metadata(const char *image_id, image_info_t *image) {
    char metadata_path[MAX_PATH_LEN];
    FILE *fp;
    char line[1024];
    
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", METADATA_DIR, image_id);
    
    fp = fopen(metadata_path, "r");
    if (!fp) {
        return -1;
    }
    
    // Simple JSON parsing (in a real implementation, use a proper JSON library)
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "\"id\"")) {
            sscanf(line, "  \"id\": \"%[^\"]\"", image->id);
        } else if (strstr(line, "\"name\"")) {
            sscanf(line, "  \"name\": \"%[^\"]\"", image->name);
        } else if (strstr(line, "\"tag\"")) {
            sscanf(line, "  \"tag\": \"%[^\"]\"", image->tag);
        } else if (strstr(line, "\"created\"")) {
            sscanf(line, "  \"created\": \"%[^\"]\"", image->created);
        } else if (strstr(line, "\"size\"")) {
            sscanf(line, "  \"size\": \"%[^\"]\"", image->size);
        }
    }
    
    fclose(fp);
    return 0;
}

int calculate_directory_size(const char *path) {
    char cmd[1024];
    FILE *fp;
    int size = 0;
    
    snprintf(cmd, sizeof(cmd), "du -sb %s 2>/dev/null | cut -f1", path);
    fp = popen(cmd, "r");
    if (fp) {
        fscanf(fp, "%d", &size);
        pclose(fp);
    }
    
    return size;
}

int create_image(const char *name, const char *tag, const char *dockerfile_path, const char *context_path) {
    image_info_t image;
    char layer_id[MAX_LAYER_ID_LEN];
    char image_path[MAX_PATH_LEN];
    int size;
    
    // Initialize image structure
    memset(&image, 0, sizeof(image));
    strcpy(image.id, generate_image_id());
    strncpy(image.name, name, sizeof(image.name) - 1);
    strncpy(image.tag, tag ? tag : "latest", sizeof(image.tag) - 1);
    strcpy(image.architecture, "amd64");
    strcpy(image.os, "linux");
    strcpy(image.author, "docker-clone");
    snprintf(image.created, sizeof(image.created), "%ld", time(NULL));
    
    // Create base layer
    strcpy(layer_id, generate_layer_id());
    if (create_layer(NULL, "FROM scratch", context_path) != 0) {
        return -1;
    }
    
    // Store layer info
    image.layers = malloc(sizeof(layer_info_t));
    if (!image.layers) {
        perror("malloc");
        return -1;
    }
    
    strcpy(image.layers[0].id, layer_id);
    strcpy(image.layers[0].command, "FROM scratch");
    image.layer_count = 1;
    
    // Calculate image size
    snprintf(image_path, sizeof(image_path), "%s/%s", LAYER_STORAGE_DIR, layer_id);
    size = calculate_directory_size(image_path);
    snprintf(image.size, sizeof(image.size), "%d", size);
    
    // Write metadata
    if (write_image_metadata(&image) != 0) {
        free(image.layers);
        return -1;
    }
    
    free(image.layers);
    return 0;
}

image_list_t* list_images() {
    DIR *dir;
    struct dirent *entry;
    image_list_t *list;
    image_info_t *images;
    int count = 0;
    
    list = malloc(sizeof(image_list_t));
    if (!list) {
        perror("malloc");
        return NULL;
    }
    
    // Count metadata files
    dir = opendir(METADATA_DIR);
    if (!dir) {
        perror("opendir metadata");
        free(list);
        return NULL;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            count++;
        }
    }
    closedir(dir);
    
    if (count == 0) {
        list->images = NULL;
        list->count = 0;
        list->capacity = 0;
        return list;
    }
    
    images = malloc(sizeof(image_info_t) * count);
    if (!images) {
        perror("malloc");
        free(list);
        return NULL;
    }
    
    // Read metadata files
    dir = opendir(METADATA_DIR);
    count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            char metadata_path[MAX_PATH_LEN];
            snprintf(metadata_path, sizeof(metadata_path), "%s/%s", METADATA_DIR, entry->d_name);
            
            if (read_image_metadata(metadata_path, &images[count]) == 0) {
                count++;
            }
        }
    }
    closedir(dir);
    
    list->images = images;
    list->count = count;
    list->capacity = count;
    
    return list;
}

image_info_t* get_image_info(const char *image_id) {
    image_info_t *image;
    
    image = malloc(sizeof(image_info_t));
    if (!image) {
        perror("malloc");
        return NULL;
    }
    
    if (read_image_metadata(image_id, image) != 0) {
        free(image);
        return NULL;
    }
    
    return image;
}

int remove_image(const char *image_id) {
    char metadata_path[MAX_PATH_LEN];
    char layer_path[MAX_PATH_LEN];
    
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.json", METADATA_DIR, image_id);
    snprintf(layer_path, sizeof(layer_path), "%s/%s", LAYER_STORAGE_DIR, image_id);
    
    // Remove metadata
    if (unlink(metadata_path) != 0) {
        perror("unlink metadata");
        return -1;
    }
    
    // Remove layer directory
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", layer_path);
    if (system(rm_cmd) != 0) {
        fprintf(stderr, "Failed to remove layer directory\n");
        return -1;
    }
    
    return 0;
}

int tag_image(const char *image_id, const char *name, const char *tag) {
    image_info_t *image;
    
    image = get_image_info(image_id);
    if (!image) {
        return -1;
    }
    
    strncpy(image->name, name, sizeof(image->name) - 1);
    strncpy(image->tag, tag ? tag : "latest", sizeof(image->tag) - 1);
    
    if (write_image_metadata(image) != 0) {
        free(image);
        return -1;
    }
    
    free(image);
    return 0;
}

int cleanup_image_system() {
    char rm_cmd[1024];
    
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s %s %s", 
             IMAGE_STORAGE_DIR, LAYER_STORAGE_DIR, METADATA_DIR);
    
    if (system(rm_cmd) != 0) {
        fprintf(stderr, "Failed to cleanup image system\n");
        return -1;
    }
    
    return 0;
}


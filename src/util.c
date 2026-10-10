#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *read_file(const char *path, size_t *len) {

    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    char *data = NULL;
    long size = (fseek(file, 0, SEEK_END) == 0) ? ftell(file) : -1;

    if (size > 0 && fseek(file, 0, SEEK_SET) == 0) {
        data = malloc((size_t)size);
        if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
            free(data);
            data = NULL;
        }
    }

    fclose(file);

    *len = data ? (size_t)size : 0;
    return data;
}

char *path_join_dir(const char *base_file, const char *relative) {

    const char *slash = strrchr(base_file, '/');
    size_t dir_len = slash ? (size_t)(slash - base_file) + 1 : 0;
    size_t relative_len = strlen(relative);

    char *path = malloc(dir_len + relative_len + 1);
    if (path) {
        memcpy(path, base_file, dir_len);
        memcpy(path + dir_len, relative, relative_len + 1);
    }

    return path;
}

#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include <tinyobj_loader_c.h>


bool mesh_validate(const Mesh *mesh) {

    if (mesh->vertex_count > 0 && !mesh->vertex_buffer) {
        fprintf(stderr, "Mesh has %zu vertices but no vertex buffer\n", mesh->vertex_count);
        return false;
    }

    if (mesh->index_count > 0 && !mesh->index_buffer) {
        fprintf(stderr, "Mesh has %zu indices but no index buffer\n", mesh->index_count);
        return false;
    }

    if (mesh->index_count % 3 != 0) {
        fprintf(stderr, "Mesh index count %zu is not a multiple of 3\n", mesh->index_count);
        return false;
    }

    for (size_t i = 0; i < mesh->index_count; ++i) {

        uint32_t index = mesh->index_buffer[i];
        if (index >= mesh->vertex_count) {
            fprintf(stderr, "Mesh index %zu is %u, but there are only %zu vertices\n", i, index, mesh->vertex_count);
            return false;
        }
    }

    return true;
}

// tinyobj keeps pointers into the file contents until parsing ends, so every buffer it asks for is kept here and freed afterwards
typedef struct {
    char **buffers;
    size_t count;
} FileBuffers;

static char *read_file(const char *path, size_t *len) {

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

static void tinyobj_file_reader(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len) {

    (void)is_mtl;
    (void)obj_filename;

    FileBuffers *files = ctx;
    *buf = NULL;
    *len = 0;

    char **grown = realloc(files->buffers, (files->count + 1) * sizeof(char *));
    if (!grown) {
        return;
    }
    files->buffers = grown;

    char *data = read_file(filename, len);
    if (data) {
        files->buffers[files->count++] = data;
        *buf = data;
    }
}

bool mesh_load_obj(Mesh *mesh, const char *path) {

    *mesh = (Mesh){0};

    tinyobj_attrib_t attrib;
    tinyobj_shape_t *shapes = NULL;
    size_t shape_count = 0;
    tinyobj_material_t *materials = NULL;
    size_t material_count = 0;
    FileBuffers files = {0};

    int result = tinyobj_parse_obj(&attrib, &shapes, &shape_count, &materials, &material_count, path, tinyobj_file_reader, &files, TINYOBJ_FLAG_TRIANGULATE);

    for (size_t i = 0; i < files.count; ++i) {
        free(files.buffers[i]);
    }
    free(files.buffers);

    if (result != TINYOBJ_SUCCESS) {
        fprintf(stderr, "Failed to load OBJ '%s' (error %d)\n", path, result);
        return false;
    }

    // After triangulation every face has 3 corners, so num_faces counts face corners, one index each
    mesh->vertex_count = attrib.num_vertices;
    mesh->index_count = attrib.num_faces;
    mesh->vertex_buffer = malloc(mesh->vertex_count * sizeof(Vertex));
    mesh->index_buffer = malloc(mesh->index_count * sizeof(uint32_t));

    bool ok = mesh->vertex_buffer && mesh->index_buffer;

    if (ok) {

        vec3s white = {.r = 255, .g = 255, .b = 255};

        for (size_t i = 0; i < mesh->vertex_count; ++i) {
            float *pos = &attrib.vertices[3 * i];
            mesh->vertex_buffer[i] = (Vertex){.pos = {.x = pos[0], .y = pos[1], .z = pos[2]}, .color = white};
        }

        // A missing or out of range index wraps to a huge value and is caught by mesh_validate
        for (size_t i = 0; i < mesh->index_count; ++i) {
            mesh->index_buffer[i] = (uint32_t)attrib.faces[i].v_idx;
        }

        ok = mesh_validate(mesh);
    } else {
        fprintf(stderr, "Out of memory loading OBJ '%s'\n", path);
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, shape_count);
    tinyobj_materials_free(materials, material_count);

    if (!ok) {
        mesh_free(mesh);
    }

    return ok;
}

void mesh_free(Mesh *mesh) {

    free(mesh->vertex_buffer);
    free(mesh->index_buffer);
    *mesh = (Mesh){0};
}

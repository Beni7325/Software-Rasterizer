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

vec3s attrib_vec3(const float *array, int index) {
    const float *v = &array[3 * index];
    return (vec3s){.x = v[0], .y = v[1], .z = v[2]};
}

// Area weighted average of the normals of the faces around each position, for corners with no normal in the file
void compute_position_normals(const tinyobj_attrib_t *attrib, vec3s *normals) {

    for (size_t i = 0; i < attrib->num_vertices; ++i) {
        normals[i] = glms_vec3_zero();
    }

    for (size_t i = 0; i + 2 < attrib->num_faces; i += 3) {

        int i0 = attrib->faces[i].v_idx;
        int i1 = attrib->faces[i + 1].v_idx;
        int i2 = attrib->faces[i + 2].v_idx;

        vec3s p0 = attrib_vec3(attrib->vertices, i0);
        vec3s p1 = attrib_vec3(attrib->vertices, i1);
        vec3s p2 = attrib_vec3(attrib->vertices, i2);

        // Counter clockwise faces point the cross product outward, and its length is twice the area
        vec3s e1 = glms_vec3_sub(p1, p0);
        vec3s e2 = glms_vec3_sub(p2, p0);
        vec3s face_normal = glms_vec3_cross(e1, e2);

        normals[i0] = glms_vec3_add(normals[i0], face_normal);
        normals[i1] = glms_vec3_add(normals[i1], face_normal);
        normals[i2] = glms_vec3_add(normals[i2], face_normal);
    }

    for (size_t i = 0; i < attrib->num_vertices; ++i) {
        normals[i] = glms_vec3_normalize(normals[i]);
    }
}

// One mesh vertex per unique (position, normal) index pair, found through an open addressing hash table
typedef struct {
    int v_idx;  // -1 marks an empty slot
    int vn_idx; // -1 when the corner uses the computed position normal
    uint32_t vertex;
} CornerSlot;

size_t corner_slot(const CornerSlot *slots, size_t capacity, int v_idx, int vn_idx) {

    uint32_t hash = (uint32_t)v_idx * 2654435761u ^ (uint32_t)vn_idx * 2246822519u;
    size_t slot = hash & (capacity - 1);

    while (slots[slot].v_idx != -1 && (slots[slot].v_idx != v_idx || slots[slot].vn_idx != vn_idx)) {
        slot = (slot + 1) & (capacity - 1);
    }

    return slot;
}

bool build_mesh(Mesh *mesh, const tinyobj_attrib_t *attrib, const char *path) {

    // After triangulation every face has 3 corners, so num_faces counts face corners, one index each
    size_t corner_count = attrib->num_faces;

    for (size_t i = 0; i < corner_count; ++i) {
        int v_idx = attrib->faces[i].v_idx;
        if (v_idx < 0 || (size_t)v_idx >= attrib->num_vertices) {
            fprintf(stderr, "OBJ '%s' corner %zu has invalid position index %d\n", path, i, v_idx);
            return false;
        }
    }

    // Keep the table at most half full so probes stay short
    size_t capacity = 1;
    while (capacity < 2 * corner_count) {
        capacity *= 2;
    }

    vec3s *position_normals = malloc(attrib->num_vertices * sizeof(vec3s));
    CornerSlot *slots = malloc(capacity * sizeof(CornerSlot));
    mesh->vertex_buffer = malloc(corner_count * sizeof(Vertex)); // Upper bound, every corner unique
    mesh->index_buffer = malloc(corner_count * sizeof(uint32_t));

    bool ok = position_normals && slots && mesh->vertex_buffer && mesh->index_buffer;

    if (ok) {

        compute_position_normals(attrib, position_normals);

        for (size_t i = 0; i < capacity; ++i) {
            slots[i].v_idx = -1;
        }

        for (size_t i = 0; i < corner_count; ++i) {

            int v_idx = attrib->faces[i].v_idx;
            int vn_idx = attrib->faces[i].vn_idx;

            // Missing normals come back as negative indices
            bool has_normal = vn_idx >= 0 && (size_t)vn_idx < attrib->num_normals;
            if (!has_normal) {
                vn_idx = -1;
            }

            size_t slot = corner_slot(slots, capacity, v_idx, vn_idx);

            if (slots[slot].v_idx == -1) {

                vec3s pos = attrib_vec3(attrib->vertices, v_idx);
                vec3s normal = has_normal ? glms_vec3_normalize(attrib_vec3(attrib->normals, vn_idx)) : position_normals[v_idx];

                uint32_t vertex = (uint32_t)mesh->vertex_count++;
                mesh->vertex_buffer[vertex] = (Vertex){.pos = pos, .normal = normal};
                slots[slot] = (CornerSlot){.v_idx = v_idx, .vn_idx = vn_idx, .vertex = vertex};
            }

            mesh->index_buffer[mesh->index_count++] = slots[slot].vertex;
        }

        ok = mesh_validate(mesh);
    } else {
        fprintf(stderr, "Out of memory loading OBJ '%s'\n", path);
    }

    free(position_normals);
    free(slots);

    return ok;
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

    bool ok = build_mesh(mesh, &attrib, path);

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

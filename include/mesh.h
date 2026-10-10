#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cglm/struct.h>


typedef struct {
    vec3s pos;
    vec3s normal;
} Vertex;

typedef struct {
    size_t vertex_count;
    size_t index_count;
    Vertex *vertex_buffer;
    uint32_t *index_buffer;
} Mesh;

bool mesh_validate(const Mesh *mesh);
bool mesh_load_obj(Mesh *mesh, const char *path);
void mesh_free(Mesh *mesh);

#endif /* MESH_H */

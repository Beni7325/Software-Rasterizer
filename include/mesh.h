#ifndef MESH_H
#define MESH_H

#include <stddef.h>

#include <cglm/struct.h>


typedef struct {
    vec3s pos;
    vec3s color;
} Vertex;

typedef struct {
    size_t vertex_count;
    Vertex *vertex_buffer;
} Mesh;

#endif /* MESH_H */

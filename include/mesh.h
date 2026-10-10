#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cglm/struct.h>
#include "texture.h"


// Loaded meshes are centered on the origin and scaled so their longest side is this long
#define NORMALIZED_MESH_SIZE 10.0f

typedef struct {
    vec3s pos;
    vec3s normal;
    vec2s uv;
} Vertex;

typedef struct {
    vec3s diffuse;           // Used when there is no diffuse texture
    Texture diffuse_texture; // texels is NULL when the material has none
} Material;

typedef struct {
    size_t vertex_count;
    size_t index_count;
    size_t material_count;
    Vertex *vertex_buffer;
    uint32_t *index_buffer;
    Material *materials;
    uint32_t *material_ids;
} Mesh;

bool mesh_validate(const Mesh *mesh);
bool mesh_load_obj(Mesh *mesh, const char *path);
void mesh_free(Mesh *mesh);

#endif /* MESH_H */

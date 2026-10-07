#include "mesh.h"

#include <stdio.h>


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

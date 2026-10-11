#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include "util.h"

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

    size_t triangle_count = mesh->index_count / 3;
    if (triangle_count > 0 && !mesh->material_ids) {
        fprintf(stderr, "Mesh has %zu triangles but no material ids\n", triangle_count);
        return false;
    }

    for (size_t i = 0; i < triangle_count; ++i) {

        uint32_t material_id = mesh->material_ids[i];
        if (material_id >= mesh->material_count) {
            fprintf(stderr, "Mesh triangle %zu uses material %u, but there are only %zu materials\n", i, material_id, mesh->material_count);
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

// Area weighted average of the normals of the faces around each position, for corners with no normal in the file
void compute_position_normals(const tinyobj_attrib_t *attrib, vec3s *normals) {

    for (size_t i = 0; i < attrib->num_vertices; ++i) {
        normals[i] = glms_vec3_zero();
    }

    for (size_t i = 0; i + 2 < attrib->num_faces; i += 3) {

        int i0 = attrib->faces[i].v_idx;
        int i1 = attrib->faces[i + 1].v_idx;
        int i2 = attrib->faces[i + 2].v_idx;

        vec3s p0 = vec3_from_array(attrib->vertices, i0);
        vec3s p1 = vec3_from_array(attrib->vertices, i1);
        vec3s p2 = vec3_from_array(attrib->vertices, i2);

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

// One mesh vertex per unique (position, normal, uv) index triple, found through an open addressing hash table
typedef struct {
    int v_idx;  // -1 marks an empty slot
    int vn_idx; // -1 when the corner uses the computed position normal
    int vt_idx; // -1 when the corner has no uv
    uint32_t vertex;
} CornerSlot;

size_t corner_slot(const CornerSlot *slots, size_t capacity, int v_idx, int vn_idx, int vt_idx) {

    uint32_t hash = (uint32_t)v_idx * 2654435761u ^ (uint32_t)vn_idx * 2246822519u ^ (uint32_t)vt_idx * 3266489917u;
    size_t slot = hash & (capacity - 1);

    while (slots[slot].v_idx != -1 &&
           (slots[slot].v_idx != v_idx || slots[slot].vn_idx != vn_idx || slots[slot].vt_idx != vt_idx)) {
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
    size_t capacity = next_pow2(2 * corner_count);

    vec3s *position_normals = malloc(attrib->num_vertices * sizeof(vec3s));
    CornerSlot *slots = malloc(capacity * sizeof(CornerSlot));
    mesh->vertex_buffer = malloc(corner_count * sizeof(Vertex)); // Upper bound, every corner unique
    mesh->index_buffer = malloc(corner_count * sizeof(uint32_t));
    mesh->material_ids = malloc(corner_count / 3 * sizeof(uint32_t));

    bool ok = position_normals && slots && mesh->vertex_buffer && mesh->index_buffer && mesh->material_ids;

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

            int vt_idx = attrib->faces[i].vt_idx;
            bool has_uv = vt_idx >= 0 && (size_t)vt_idx < attrib->num_texcoords;
            if (!has_uv) {
                vt_idx = -1;
            }

            size_t slot = corner_slot(slots, capacity, v_idx, vn_idx, vt_idx);

            if (slots[slot].v_idx == -1) {

                vec3s pos = vec3_from_array(attrib->vertices, v_idx);
                vec3s normal = has_normal ? glms_vec3_normalize(vec3_from_array(attrib->normals, vn_idx)) : position_normals[v_idx];
                vec2s uv = has_uv ? vec2_from_array(attrib->texcoords, vt_idx) : glms_vec2_zero();

                uint32_t vertex = (uint32_t)mesh->vertex_count++;
                mesh->vertex_buffer[vertex] = (Vertex){.pos = pos, .normal = normal, .uv = uv};
                slots[slot] = (CornerSlot){.v_idx = v_idx, .vn_idx = vn_idx, .vt_idx = vt_idx, .vertex = vertex};
            }

            mesh->index_buffer[mesh->index_count++] = slots[slot].vertex;
        }

        // Triangulated faces line up with triangles. Shift by one so OBJ material -1 (none or unknown) maps to the default material 0
        for (size_t i = 0; i < corner_count / 3; ++i) {

            int obj_material = attrib->material_ids[i];
            bool has_material = obj_material >= 0 && (size_t)obj_material + 1 < mesh->material_count;

            mesh->material_ids[i] = has_material ? (uint32_t)obj_material + 1 : 0;
        }

        ok = mesh_validate(mesh);
    } else {
        fprintf(stderr, "Out of memory loading OBJ '%s'\n", path);
    }

    free(position_normals);
    free(slots);

    return ok;
}

// Centers the mesh on the origin and uniformly scales it so its longest side is NORMALIZED_MESH_SIZE
// Uniform scaling leaves the normals valid, so only positions change
void normalize_positions(Mesh *mesh) {

    if (mesh->vertex_count == 0) {
        return;
    }

    vec3s bb_min = mesh->vertex_buffer[0].pos;
    vec3s bb_max = mesh->vertex_buffer[0].pos;

    for (size_t i = 1; i < mesh->vertex_count; ++i) {
        aabb_extend(&bb_min, &bb_max, mesh->vertex_buffer[i].pos);
    }

    vec3s center = glms_vec3_center(bb_min, bb_max);
    vec3s extent = glms_vec3_sub(bb_max, bb_min);
    float longest_side = glms_vec3_max(extent);

    // A single point has no size to normalize, just center it
    float scale = longest_side > 0.0f ? NORMALIZED_MESH_SIZE / longest_side : 1.0f;

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        vec3s centered = glms_vec3_sub(mesh->vertex_buffer[i].pos, center);
        mesh->vertex_buffer[i].pos = glms_vec3_scale(centered, scale);
    }
}

// Material 0 is a white default, OBJ material i becomes material i + 1
bool build_materials(Mesh *mesh, const tinyobj_material_t *obj_materials, size_t obj_material_count, const char *path) {

    mesh->material_count = obj_material_count + 1;
    mesh->materials = calloc(mesh->material_count, sizeof(Material));
    if (!mesh->materials) {
        fprintf(stderr, "Out of memory loading materials for OBJ '%s'\n", path);
        return false;
    }

    mesh->materials[0].diffuse = glms_vec3_one();

    for (size_t i = 0; i < obj_material_count; ++i) {

        const tinyobj_material_t *obj_material = &obj_materials[i];
        Material *material = &mesh->materials[i + 1];

        material->diffuse = (vec3s){.r = obj_material->diffuse[0], .g = obj_material->diffuse[1], .b = obj_material->diffuse[2]};

        if (!obj_material->diffuse_texname) {
            continue;
        }

        // A missing texture leaves the material with its flat diffuse color instead of failing the whole mesh
        // Texture paths in the MTL file are relative to the OBJ's directory
        char *tex_path = path_join_dir(path, obj_material->diffuse_texname);
        if (tex_path) {
            texture_load(&material->diffuse_texture, tex_path);
            free(tex_path);
        }
    }

    return true;
}

bool mesh_load_obj(Mesh *mesh, const char *path) {

    *mesh = (Mesh){0};

    tinyobj_attrib_t attrib = {0};
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

    bool ok = build_materials(mesh, materials, material_count, path) && build_mesh(mesh, &attrib, path);

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, shape_count);
    tinyobj_materials_free(materials, material_count);

    if (ok) {
        normalize_positions(mesh);
    } else {
        mesh_free(mesh);
    }

    return ok;
}

void mesh_free(Mesh *mesh) {

    for (size_t i = 0; i < mesh->material_count; ++i) {
        texture_free(&mesh->materials[i].diffuse_texture);
    }

    free(mesh->vertex_buffer);
    free(mesh->index_buffer);
    free(mesh->materials);
    free(mesh->material_ids);
    *mesh = (Mesh){0};
}

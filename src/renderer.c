#include "renderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "tile.h"
#include "util.h"


// TODO: Implement top-left rule
static void rasterize_triangle(FrameBuffer *fb, const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, const Material *material, ivec2s tile_min, ivec2s tile_max) {

    vec2s a = {.x = v0->screen.x, .y = v0->screen.y};
    vec2s b = {.x = v1->screen.x, .y = v1->screen.y};
    vec2s c = {.x = v2->screen.x, .y = v2->screen.y};

    float double_area = edge_function(a, b, c);
    float inv_double_area = 1.0f / double_area;

    vec3s depths = {.x = v0->screen.z, .y = v1->screen.z, .z = v2->screen.z};
    vec3s inv_ws = {.x = v0->screen.w, .y = v1->screen.w, .z = v2->screen.w};

    // Triangle bounding box
    vec2s bb_min = min3_vec2(a, b, c);
    vec2s bb_max = max3_vec2(a, b, c);

    // Pixels whose center (x + 0.5, y + 0.5) lies inside the box
    vec2s half_pixel = {.x = 0.5f, .y = 0.5f};
    vec2s centers_min = glms_vec2_sub(bb_min, half_pixel);
    vec2s centers_max = glms_vec2_sub(bb_max, half_pixel);

    ivec2s px_min = {.x = (int)ceilf(centers_min.x),  .y = (int)ceilf(centers_min.y)};
    ivec2s px_max = {.x = (int)floorf(centers_max.x), .y = (int)floorf(centers_max.y)};

    // Clamp to the framebuffer
    ivec2s screen_min = {.x = 0, .y = 0};
    ivec2s screen_max = {.x = fb->width - 1, .y = fb->height - 1};

    px_min = glms_ivec2_maxv(px_min, screen_min);
    px_max = glms_ivec2_minv(px_max, screen_max);

    // Clamp to the tile being rasterized
    px_min = glms_ivec2_maxv(px_min, tile_min);
    px_max = glms_ivec2_minv(px_max, tile_max);

    // Edge functions are linear: evaluate them at the top left pixel center, then step per pixel in x and y
    vec2s p_tl = {.x = px_min.x + 0.5f, .y = px_min.y + 0.5f};
    vec3s w_tl = {.x = edge_function(b, c, p_tl), .y = edge_function(c, a, p_tl), .z = edge_function(a, b, p_tl)};
    vec3s w_dx = {.x = b.y - c.y, .y = c.y - a.y, .z = a.y - b.y};
    vec3s w_dy = {.x = c.x - b.x, .y = a.x - c.x, .z = b.x - a.x};

    Varyings interpolated_varyings;

    for (int y = px_min.y; y <= px_max.y; ++y) {

        vec3s row_offset = glms_vec3_scale(w_dy, y - px_min.y);
        vec3s w_row = glms_vec3_add(w_tl, row_offset);

        for (int x = px_min.x; x <= px_max.x; ++x) {

            vec3s col_offset = glms_vec3_scale(w_dx, x - px_min.x);
            vec3s w = glms_vec3_add(w_row, col_offset);

            if (w.x >= 0 && w.y >= 0 && w.z >= 0) {

                // Interpolate depth (NDC depth is affine in screen space)
                vec3s l = glms_vec3_scale(w, inv_double_area);
                float depth = glms_vec3_dot(depths, l);

                int idx = y * fb->width + x;
                if (depth < fb->depth_buffer[idx]) {
                    fb->depth_buffer[idx] = depth;

                    // Perspective-correct interpolation of varyings
                    vec3s l_over_w = glms_vec3_mul(l, inv_ws);
                    float inv_w = glms_vec3_hadd(l_over_w);
                    vec3s l_persp = glms_vec3_divs(l_over_w, inv_w);

                    varyings_interpolate(&v0->v, &v1->v, &v2->v, l_persp, &interpolated_varyings);
                    fb->color_buffer[idx] = fragment_shader(&interpolated_varyings, material);
                }
            }
        }
    }
}

static vec4s clip_to_viewport(const FrameBuffer *fb, vec4s clip) {

    float inv_w = 1.0f / clip.w;
    vec3s clip_xyz = glms_vec3(clip);
    vec3s ndc = glms_vec3_scale(clip_xyz, inv_w);

    // NDC y points up, framebuffer rows go down
    vec4s screen = {
        .x = (ndc.x + 1.0f) * 0.5f * fb->width,
        .y = (1.0f - ndc.y) * 0.5f * fb->height,
        .z = ndc.z,
        .w = inv_w,
    };

    return screen;
}

enum {
    FRUSTUM_OUTSIDE_LEFT   = 1 << 0,
    FRUSTUM_OUTSIDE_RIGHT  = 1 << 1,
    FRUSTUM_OUTSIDE_BOTTOM = 1 << 2,
    FRUSTUM_OUTSIDE_TOP    = 1 << 3,
    FRUSTUM_OUTSIDE_NEAR   = 1 << 4,
    FRUSTUM_OUTSIDE_FAR    = 1 << 5,
};

uint8_t frustum_outside_mask(vec4s clip) {

    uint8_t mask = 0;

    if (clip.x < -clip.w) mask |= FRUSTUM_OUTSIDE_LEFT;
    if (clip.x >  clip.w) mask |= FRUSTUM_OUTSIDE_RIGHT;
    if (clip.y < -clip.w) mask |= FRUSTUM_OUTSIDE_BOTTOM;
    if (clip.y >  clip.w) mask |= FRUSTUM_OUTSIDE_TOP;
    if (clip.z < -clip.w) mask |= FRUSTUM_OUTSIDE_NEAR;
    if (clip.z >  clip.w) mask |= FRUSTUM_OUTSIDE_FAR;

    return mask;
}

#define FRUSTUM_PLANE_COUNT 6

static const vec4s frustum_planes[FRUSTUM_PLANE_COUNT] = {
    {{ 1.0f,  0.0f,  0.0f, 1.0f}}, // Left:   w + x >= 0
    {{-1.0f,  0.0f,  0.0f, 1.0f}}, // Right:  w - x >= 0
    {{ 0.0f,  1.0f,  0.0f, 1.0f}}, // Bottom: w + y >= 0
    {{ 0.0f, -1.0f,  0.0f, 1.0f}}, // Top:    w - y >= 0
    {{ 0.0f,  0.0f,  1.0f, 1.0f}}, // Near:   w + z >= 0
    {{ 0.0f,  0.0f, -1.0f, 1.0f}}, // Far:    w - z >= 0
};

#define MAX_CLIPPED_VERTICES (3 + FRUSTUM_PLANE_COUNT)

// Clips a triangle against the frustum planes set in plane_mask, in clip space. Planes the triangle is fully inside of can be left out of the mask to skip them
// Writes the resulting convex polygon to out and returns its vertex count
int clip_triangle_to_frustum(const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, unsigned plane_mask, VertexShaderOutput out[MAX_CLIPPED_VERTICES]) {

    VertexShaderOutput buffer_a[MAX_CLIPPED_VERTICES] = {*v0, *v1, *v2};
    VertexShaderOutput buffer_b[MAX_CLIPPED_VERTICES];
    VertexShaderOutput *src = buffer_a;
    VertexShaderOutput *dst = buffer_b;
    int src_count = 3;

    for (int plane = 0; plane < FRUSTUM_PLANE_COUNT; ++plane) {

        // Bit order matches frustum_planes
        unsigned plane_bit = 1u << plane;
        if (!(plane_mask & plane_bit)) {
            continue;
        }

        // Side planes are pushed out to a guard band of 8w, the rasterizer's bbox clamp handles the off screen parts inside of it.
        // Past it screen coordinates get too large for float edge functions. Near and far stay on the real frustum
        const float guard_band_scale = 8.0f;
        unsigned side_bits = FRUSTUM_OUTSIDE_LEFT | FRUSTUM_OUTSIDE_RIGHT | FRUSTUM_OUTSIDE_BOTTOM | FRUSTUM_OUTSIDE_TOP;
        vec4s clip_plane = frustum_planes[plane];
        if (plane_bit & side_bits) {
            clip_plane.w = guard_band_scale;
        }

        int dst_count = 0;

        for (int i = 0; i < src_count; ++i) {

            VertexShaderOutput *curr = &src[i];
            VertexShaderOutput *next = &src[(i + 1) % src_count];

            float d_curr = glms_vec4_dot(curr->clip, clip_plane);
            float d_next = glms_vec4_dot(next->clip, clip_plane);

            if (d_curr >= 0) {
                dst[dst_count++] = *curr;
            }

            if ((d_curr >= 0) != (d_next >= 0)) {
                // Edge crosses the plane, emit the intersection
                float t = d_curr / (d_curr - d_next);

                VertexShaderOutput intersection;
                intersection.clip = glms_vec4_lerp(curr->clip, next->clip, t);
                intersection.v.normal = glms_vec3_lerp(curr->v.normal, next->v.normal, t);
                intersection.v.uv = glms_vec2_lerp(curr->v.uv, next->v.uv, t);

                dst[dst_count++] = intersection;
            }
        }

        // Fully outside this plane
        if (dst_count == 0) {
            return 0;
        }

        VertexShaderOutput *tmp = src;
        src = dst;
        dst = tmp;
        src_count = dst_count;
    }

    memcpy(out, src, src_count * sizeof(VertexShaderOutput));

    return src_count;
}

#define DEGENERATE_DOUBLE_AREA 1e-6f

bool setup_triangle(const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2) {

    vec2s a = {.x = v0->screen.x, .y = v0->screen.y};
    vec2s b = {.x = v1->screen.x, .y = v1->screen.y};
    vec2s c = {.x = v2->screen.x, .y = v2->screen.y};

    float double_area = edge_function(a, b, c);

    // Cull back faces (negative area) and degenerate triangles (zero or near zero arear)
    if (double_area < DEGENERATE_DOUBLE_AREA) {
        return false;
    }

    return true;
}

void draw_mesh(FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms) {

    // Triangle ids below this are the mesh's triangles, the rest are clip triangle id - mesh_triangle_count
    size_t mesh_triangle_count = mesh->index_count / 3;

    // Clipped polygons go in their own vertex buffer, 50% of the shaded vertices covers the clipping seen in tests.
    // The new triangles created after clipping go in clip_index_buffer (3 indices into clip_v_out each) and clip_material_ids (1 each).
    size_t clip_capacity = mesh->vertex_count / 2;
    size_t clip_vertex_count = 0;
    size_t clip_triangle_count = 0;

    // TODO: Use some kind of arena to avoid calling malloc each time
    VertexShaderOutput *v_out = malloc(sizeof(VertexShaderOutput) * mesh->vertex_count);
    VertexShaderOutput *clip_v_out = malloc(sizeof(VertexShaderOutput) * clip_capacity);
    uint32_t *clip_index_buffer = malloc(sizeof(uint32_t) * 3 * clip_capacity);
    uint32_t *clip_material_ids = malloc(sizeof(uint32_t) * clip_capacity);
    TileBins tile_bins = {0};
    if (!v_out || !clip_v_out || !clip_index_buffer || !clip_material_ids || !tile_bins_init(&tile_bins, fb->width, fb->height)) {
        free(v_out);
        free(clip_v_out);
        free(clip_index_buffer);
        free(clip_material_ids);
        tile_bins_free(&tile_bins);
        return;
    }

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        vertex_shader(&mesh->vertex_buffer[i], uniforms, &v_out[i]);
        v_out[i].screen = clip_to_viewport(fb, v_out[i].clip);
    }

    // Step one triangle (3 indices) at a time
    for (size_t i = 0; i + 2 < mesh->index_count; i += 3) {

        uint32_t i0 = mesh->index_buffer[i];
        uint32_t i1 = mesh->index_buffer[i + 1];
        uint32_t i2 = mesh->index_buffer[i + 2];

        // All three vertices outside the same plane means the whole triangle is, so skip it.
        // TODO: Masks are recomputed for shared vertices
        uint8_t mask0 = frustum_outside_mask(v_out[i0].clip);
        uint8_t mask1 = frustum_outside_mask(v_out[i1].clip);
        uint8_t mask2 = frustum_outside_mask(v_out[i2].clip);

        uint8_t shared_outside = mask0 & mask1 & mask2;
        if (shared_outside) {
            continue;
        }

        // Only planes crossed by at least one vertex need clipping
        uint8_t any_outside = mask0 | mask1 | mask2;

        // Every triangle the clipper fans out shares the source triangle's material
        uint32_t triangle_id = (uint32_t)(i / 3);
        uint32_t material_id = mesh->material_ids[triangle_id];

        // Front faces are counter clock wise in NDC but the viewport y flip makes them clock wise on screen
        // Reverse the order here so front faces end up with positive screen area
        const VertexShaderOutput *v0 = &v_out[i0];
        const VertexShaderOutput *v1 = &v_out[i2];
        const VertexShaderOutput *v2 = &v_out[i1];

        // Fully inside, the vertices are already in screen space so use them in place
        if (!any_outside) {
            if (setup_triangle(v0, v1, v2)) {
                vec2s a = {.x = v0->screen.x, .y = v0->screen.y};
                vec2s b = {.x = v1->screen.x, .y = v1->screen.y};
                vec2s c = {.x = v2->screen.x, .y = v2->screen.y};

                bin_triangle(&tile_bins, a, b, c, triangle_id);
            }
            continue;
        }

        // The clipper writes the whole polygon, kept vertices included, so it can need up to MAX_CLIPPED_VERTICES slots.
        // TODO: In our testing the clip buffers are big enough, triangles that don't fit are dropped for now and will be handled later
        if (clip_capacity - clip_vertex_count < MAX_CLIPPED_VERTICES) {
            continue;
        }

        uint32_t polygon_start = (uint32_t)clip_vertex_count;
        VertexShaderOutput *clipped = &clip_v_out[polygon_start];

        int clipped_count = clip_triangle_to_frustum(v0, v1, v2, any_outside, clipped);
        clip_vertex_count += clipped_count;

        // Clipping creates new vertices, so the divide has to happen after it
        for (int j = 0; j < clipped_count; ++j) {
            clipped[j].screen = clip_to_viewport(fb, clipped[j].clip);
        }

        // Fan triangulate the clipped polygon
        for (int j = 1; j + 1 < clipped_count; ++j) {

            uint32_t c0_index = polygon_start;
            uint32_t c1_index = polygon_start + j;
            uint32_t c2_index = polygon_start + j + 1;

            const VertexShaderOutput *c0 = &clip_v_out[c0_index];
            const VertexShaderOutput *c1 = &clip_v_out[c1_index];
            const VertexShaderOutput *c2 = &clip_v_out[c2_index];

            if (setup_triangle(c0, c1, c2)) {
                clip_index_buffer[3 * clip_triangle_count] = c0_index;
                clip_index_buffer[3 * clip_triangle_count + 1] = c2_index;
                clip_index_buffer[3 * clip_triangle_count + 2] = c1_index;
                clip_material_ids[clip_triangle_count] = material_id;

                uint32_t clip_triangle_id = (uint32_t)(mesh_triangle_count + clip_triangle_count);
                clip_triangle_count++;

                vec2s a = {.x = c0->screen.x, .y = c0->screen.y};
                vec2s b = {.x = c1->screen.x, .y = c1->screen.y};
                vec2s c = {.x = c2->screen.x, .y = c2->screen.y};

                bin_triangle(&tile_bins, a, b, c, clip_triangle_id);
            }
        }
    }

    for (int ty = 0; ty < tile_bins.tiles.y; ++ty) {
        for (int tx = 0; tx < tile_bins.tiles.x; ++tx) {

            ivec2s tile_min = {.x = tx * TILE_SIZE, .y = ty * TILE_SIZE};
            ivec2s tile_max = {.x = tile_min.x + TILE_SIZE - 1, .y = tile_min.y + TILE_SIZE - 1};

            const BinChunk *chunk = tile_bins.bins[ty * tile_bins.tiles.x + tx];

            for (; chunk; chunk = chunk->next) {
                for (uint32_t k = 0; k < chunk->count; ++k) {

                    uint32_t triangle_id = chunk->triangle_ids[k];

                    // Past the mesh's triangles the indices and vertices are in the clip buffers
                    const uint32_t *indices;
                    const VertexShaderOutput *vertices;
                    uint32_t material_id;
                    if (triangle_id < mesh_triangle_count) {
                        indices = &mesh->index_buffer[3 * triangle_id];
                        vertices = v_out;
                        material_id = mesh->material_ids[triangle_id];
                    } else {
                        uint32_t clip_triangle = triangle_id - mesh_triangle_count;
                        indices = &clip_index_buffer[3 * clip_triangle];
                        vertices = clip_v_out;
                        material_id = clip_material_ids[clip_triangle];
                    }

                    // Same winding swap as when the triangle was binned
                    const VertexShaderOutput *v0 = &vertices[indices[0]];
                    const VertexShaderOutput *v1 = &vertices[indices[2]];
                    const VertexShaderOutput *v2 = &vertices[indices[1]];
                    const Material *material = &mesh->materials[material_id];

                    rasterize_triangle(fb, v0, v1, v2, material, tile_min, tile_max);
                }
            }
        }
    }

    tile_bins_free(&tile_bins);
    free(clip_material_ids);
    free(clip_index_buffer);
    free(clip_v_out);
    free(v_out);
}

void draw_model(FrameBuffer *fb, const Model *model, const Camera *camera) {

    mat4s model_mat = model_matrix(model);
    Uniforms uniforms = {.mvp = glms_mat4_mul(camera->view_projection, model_mat)};

    draw_mesh(fb, model->mesh, &uniforms);
}

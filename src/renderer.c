#include "renderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"


// TODO: Implement top-left rule
static void rasterize_triangle(FrameBuffer *fb, const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, const Material *material) {

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

        int dst_count = 0;

        for (int i = 0; i < src_count; ++i) {

            VertexShaderOutput *curr = &src[i];
            VertexShaderOutput *next = &src[(i + 1) % src_count];

            float d_curr = glms_vec4_dot(curr->clip, frustum_planes[plane]);
            float d_next = glms_vec4_dot(next->clip, frustum_planes[plane]);

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

    // TODO: Use some kind of arena to avoid calling malloc each time
    VertexShaderOutput *v_out = malloc(sizeof(VertexShaderOutput) * mesh->vertex_count);
    if (!v_out) {
        return;
    }

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        vertex_shader(&mesh->vertex_buffer[i], uniforms, &v_out[i]);
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
        uint32_t material_id = mesh->material_ids[i / 3];
        const Material *material = &mesh->materials[material_id];

        // Front faces are counter clock wise in NDC but the viewport y flip makes them clock wise on screen
        // Reverse the order here so front faces end up with positive screen area
        const VertexShaderOutput *v0 = &v_out[i0];
        const VertexShaderOutput *v1 = &v_out[i2];
        const VertexShaderOutput *v2 = &v_out[i1];

        VertexShaderOutput clipped[MAX_CLIPPED_VERTICES];
        int clipped_count = clip_triangle_to_frustum(v0, v1, v2, any_outside, clipped);

        // Clipping creates new vertices, so the divide has to happen after it
        for (int j = 0; j < clipped_count; ++j) {
            clipped[j].screen = clip_to_viewport(fb, clipped[j].clip);
        }

        // Fan triangulate the clipped polygon
        for (int j = 1; j + 1 < clipped_count; ++j) {

            const VertexShaderOutput *c0 = &clipped[0];
            const VertexShaderOutput *c1 = &clipped[j];
            const VertexShaderOutput *c2 = &clipped[j + 1];

            if (setup_triangle(c0, c1, c2)) {
                rasterize_triangle(fb, c0, c1, c2, material);
            }
        }
    }

    free(v_out);
}

void draw_model(FrameBuffer *fb, const Model *model, const Camera *camera) {

    mat4s model_mat = model_matrix(model);
    Uniforms uniforms = {.mvp = glms_mat4_mul(camera->view_projection, model_mat)};

    draw_mesh(fb, model->mesh, &uniforms);
}

#include "renderer.h"

#include <math.h>
#include <stdlib.h>


static float edge(vec2s a, vec2s b, vec2s p) {

    vec2s ab = glms_vec2_sub(b, a);
    vec2s ap = glms_vec2_sub(p, a);

    return glms_vec2_cross(ab, ap);
}

// TODO: Implement top-left rule
static void rasterize_triangle(FrameBuffer *fb, const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2) {

    vec2s a = {.x = v0->screen.x, .y = v0->screen.y};
    vec2s b = {.x = v1->screen.x, .y = v1->screen.y};
    vec2s c = {.x = v2->screen.x, .y = v2->screen.y};

    float double_area = edge(a, b, c);

    mat3s colors = {.col = {v0->v.color, v1->v.color, v2->v.color}};
    vec3s depths = {.x = v0->screen.z, .y = v1->screen.z, .z = v2->screen.z};
    vec3s inv_ws = {.x = v0->screen.w, .y = v1->screen.w, .z = v2->screen.w};

    // Triangle bounding box
    vec2s bb_min = glms_vec2_minv(a, glms_vec2_minv(b, c));
    vec2s bb_max = glms_vec2_maxv(a, glms_vec2_maxv(b, c));

    // Clamp to the framebuffer
    vec2s screen_min = {.x = 0.0f, .y = 0.0f };
    vec2s screen_max = {.x = fb->width - 1.0f, .y = fb->height - 1.0f};

    bb_min = glms_vec2_maxv(bb_min, screen_min);
    bb_max = glms_vec2_minv(bb_max, screen_max);

    // Pixel range covered by the box
    // TODO: Tighten to pixel centers
    ivec2s px_min = {.x = (int)floorf(bb_min.x), .y = (int)floorf(bb_min.y)};
    ivec2s px_max = {.x = (int)ceilf(bb_max.x),  .y = (int)ceilf(bb_max.y)};

    Varyings interpolated_varyings;

    for (int y = px_min.y; y <= px_max.y; ++y) {
        for (int x = px_min.x; x <= px_max.x; ++x) {

            // Sample at pixel center
            vec2s p = {.x = x + 0.5f, .y = y + 0.5f};
            vec3s w = {
                .x = edge(b, c, p),
                .y = edge(c, a, p),
                .z = edge(a, b, p),
            };

            if (w.x >= 0 && w.y >= 0 && w.z >= 0) {

                // Interpolate depth (NDC depth is affine in screen space)
                vec3s l = glms_vec3_divs(w, double_area);
                float depth = glms_vec3_dot(depths, l);

                int idx = y * fb->width + x;
                if (depth < fb->depth_buffer[idx]) {
                    fb->depth_buffer[idx] = depth;

                    // Perspective-correct interpolation of varyings
                    vec3s l_over_w = glms_vec3_mul(l, inv_ws);
                    float inv_w = glms_vec3_hadd(l_over_w);
                    vec3s l_persp = glms_vec3_divs(l_over_w, inv_w);

                    interpolated_varyings.color = glms_mat3_mulv(colors, l_persp);
                    fb->color_buffer[idx] = fragment_shader(&interpolated_varyings);
                }
            }
        }
    }
}

static vec4s clip_to_screen(const FrameBuffer *fb, vec4s clip) {

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

// Signed distance to the near plane in clip space, >= 0 means inside
float near_plane_distance(vec4s clip) {
    return clip.w + clip.z;
}

// Clipping one plane can add at most one vertex: 3 + 1
#define MAX_CLIPPED_VERTICES 4

// Clips a triangle against the near plane only, in clip space
// Writes the resulting convex polygon to out and returns its vertex count
int clip_triangle_near(const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, VertexShaderOutput out[MAX_CLIPPED_VERTICES]) {

    const VertexShaderOutput *in[3] = {v0, v1, v2};
    int out_count = 0;

    for (int i = 0; i < 3; ++i) {

        const VertexShaderOutput *curr = in[i];
        const VertexShaderOutput *next = in[(i + 1) % 3];

        float d_curr = near_plane_distance(curr->clip);
        float d_next = near_plane_distance(next->clip);

        if (d_curr >= 0) {
            out[out_count++] = *curr;
        }

        if ((d_curr >= 0) != (d_next >= 0)) {
            // Edge crosses the plane, emit the intersection
            float t = d_curr / (d_curr - d_next);

            VertexShaderOutput intersection;
            intersection.clip = glms_vec4_lerp(curr->clip, next->clip, t);
            intersection.v.color = glms_vec3_lerp(curr->v.color, next->v.color, t);

            out[out_count++] = intersection;
        }
    }

    return out_count;
}

#define DEGENERATE_DOUBLE_AREA 1e-6f

bool setup_triangle(const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2) {

    vec2s a = {.x = v0->screen.x, .y = v0->screen.y};
    vec2s b = {.x = v1->screen.x, .y = v1->screen.y};
    vec2s c = {.x = v2->screen.x, .y = v2->screen.y};

    float double_area = edge(a, b, c);

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

        // Front faces are counter clock wise in NDC but the viewport y flip makes them clock wise on screen
        // Reverse the order here so front faces end up with positive screen area
        const VertexShaderOutput *v0 = &v_out[mesh->index_buffer[i]];
        const VertexShaderOutput *v1 = &v_out[mesh->index_buffer[i + 2]];
        const VertexShaderOutput *v2 = &v_out[mesh->index_buffer[i + 1]];

        VertexShaderOutput clipped[MAX_CLIPPED_VERTICES];
        int clipped_count = clip_triangle_near(v0, v1, v2, clipped);

        // Clipping creates new vertices, so the divide has to happen after it
        for (int j = 0; j < clipped_count; ++j) {
            clipped[j].screen = clip_to_screen(fb, clipped[j].clip);
        }

        // Fan triangulate the clipped polygon
        for (int j = 1; j + 1 < clipped_count; ++j) {

            const VertexShaderOutput *c0 = &clipped[0];
            const VertexShaderOutput *c1 = &clipped[j];
            const VertexShaderOutput *c2 = &clipped[j + 1];

            if (setup_triangle(c0, c1, c2)) {
                rasterize_triangle(fb, c0, c1, c2);
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

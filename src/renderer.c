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
    if (double_area == 0) {
        return;
    } else if (double_area < 0) {
        vec2s tmp_v = b; b = c; c = tmp_v;
        const VertexShaderOutput *tmp_p = v1; v1 = v2; v2 = tmp_p;
        double_area = -double_area;
    }

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

void draw_mesh(FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms) {

    // TODO: Use some kind of arena to avoid calling malloc each time
    VertexShaderOutput *v_out = malloc(sizeof(VertexShaderOutput) * mesh->vertex_count);
    if (!v_out) {
        return;
    }

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        vertex_shader(&mesh->vertex_buffer[i], uniforms, &v_out[i]);
    }

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        v_out[i].screen = clip_to_screen(fb, v_out[i].clip);
    }

    size_t triangle_count = mesh->vertex_count / 3;
    for (size_t i = 0; i < triangle_count; ++i) {
        rasterize_triangle(fb, &v_out[3 * i], &v_out[3 * i + 1], &v_out[3 * i + 2]);
    }

    free(v_out);
}

void draw_model(FrameBuffer *fb, const Model *model, const Camera *camera) {

    mat4s model_mat = model_matrix(model);
    Uniforms uniforms = {.mvp = glms_mat4_mul(camera->view_projection, model_mat)};

    draw_mesh(fb, model->mesh, &uniforms);
}

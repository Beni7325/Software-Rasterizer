#include "renderer.h"

#include <math.h>
#include <stdlib.h>
#include "shader.h"


static float edge(vec2s a, vec2s b, vec2s p) {

    vec2s ab = glms_vec2_sub(b, a);
    vec2s ap = glms_vec2_sub(p, a);

    return glms_vec2_cross(ab, ap);
}

// TODO: Implement top-left rule
static void rasterize_triangle(FrameBuffer *fb, const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2) {

    vec2s a = {.x = v0->clip.x, .y = v0->clip.y};
    vec2s b = {.x = v1->clip.x, .y = v1->clip.y};
    vec2s c = {.x = v2->clip.x, .y = v2->clip.y};

    float double_area = edge(a, b, c);
    if (double_area == 0) {
        return;
    } else if (double_area < 0) {
        vec2s tmp_v = b; b = c; c = tmp_v;
        const VertexShaderOutput *tmp_p = v1; v1 = v2; v2 = tmp_p;
        double_area = -double_area;
    }

    mat3s colors = {.col = {v0->v.color, v1->v.color, v2->v.color}};

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

                // Interpolate
                vec3s l = glms_vec3_divs(w, double_area);
                interpolated_varyings.color = glms_mat3_mulv(colors, l);

                fb->color_buffer[y * fb->width + x] = fragment_shader(&interpolated_varyings);
            }
        }
    }
}

void draw_call(FrameBuffer *fb, Mesh *mesh) {

    // TODO: Use some kind of arena to avoid calling malloc each time
    VertexShaderOutput *v_out = malloc(sizeof(VertexShaderOutput) * mesh->vertex_count);
    if (!v_out) {
        return;
    }

    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        vertex_shader(&mesh->vertex_buffer[i], &v_out[i]);
    }

    size_t triangle_count = mesh->vertex_count / 3;
    for (size_t i = 0; i < triangle_count; ++i) {
        rasterize_triangle(fb, &v_out[3 * i], &v_out[3 * i + 1], &v_out[3 * i + 2]);
    }

    free(v_out);
}

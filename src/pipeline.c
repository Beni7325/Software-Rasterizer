#include "pipeline.h"


vec4s clip_to_viewport(const FrameBuffer *fb, vec4s clip) {

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

vec2s screen_xy(const VertexShaderOutput *v) {
    return (vec2s){.x = v->screen.x, .y = v->screen.y};
}

void fetch_triangle(const VertexShaderOutput *vertices, const uint32_t *indices, const VertexShaderOutput **v0, const VertexShaderOutput **v1, const VertexShaderOutput **v2) {

    *v0 = &vertices[indices[0]];
    *v1 = &vertices[indices[2]];
    *v2 = &vertices[indices[1]];
}

bool claim_range(atomic_size_t *next, size_t chunk_size, size_t count, size_t *start, size_t *end) {

    *start = atomic_fetch_add_explicit(next, chunk_size, memory_order_relaxed);
    if (*start >= count) {
        return false;
    }

    size_t chunk_end = *start + chunk_size;
    *end = chunk_end < count ? chunk_end : count;

    return true;
}

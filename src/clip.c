#include "clip.h"

#include <string.h>


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

static const vec4s frustum_planes[FRUSTUM_PLANE_COUNT] = {
    {{ 1.0f,  0.0f,  0.0f, 1.0f}}, // Left:   w + x >= 0
    {{-1.0f,  0.0f,  0.0f, 1.0f}}, // Right:  w - x >= 0
    {{ 0.0f,  1.0f,  0.0f, 1.0f}}, // Bottom: w + y >= 0
    {{ 0.0f, -1.0f,  0.0f, 1.0f}}, // Top:    w - y >= 0
    {{ 0.0f,  0.0f,  1.0f, 1.0f}}, // Near:   w + z >= 0
    {{ 0.0f,  0.0f, -1.0f, 1.0f}}, // Far:    w - z >= 0
};

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

#ifndef CLIP_H
#define CLIP_H

#include <stdint.h>
#include <cglm/struct.h>
#include "shader.h"


// Bit order matches the plane order used by clip_triangle_to_frustum
enum {
    FRUSTUM_OUTSIDE_LEFT   = 1 << 0,
    FRUSTUM_OUTSIDE_RIGHT  = 1 << 1,
    FRUSTUM_OUTSIDE_BOTTOM = 1 << 2,
    FRUSTUM_OUTSIDE_TOP    = 1 << 3,
    FRUSTUM_OUTSIDE_NEAR   = 1 << 4,
    FRUSTUM_OUTSIDE_FAR    = 1 << 5,
};

#define FRUSTUM_PLANE_COUNT 6

// Each plane can add at most one vertex to the polygon
#define MAX_CLIPPED_VERTICES (3 + FRUSTUM_PLANE_COUNT)

// Bitmask of the frustum planes a clip space position is outside of
uint8_t frustum_outside_mask(vec4s clip);

// Clips a triangle against the frustum planes set in plane_mask, in clip space. Planes the triangle is fully inside of can be left out of the mask to skip them
// Writes the resulting convex polygon to out and returns its vertex count
int clip_triangle_to_frustum(const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, unsigned plane_mask, VertexShaderOutput out[MAX_CLIPPED_VERTICES]);

#endif /* CLIP_H */

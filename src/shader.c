#include "shader.h"

#include <math.h>
#include "framebuffer.h"

// Normalized nit vector towards the light, (1, 1, 1)
#define INV_SQRT3 0.57735027f
static const vec3s light_dir = {.x = INV_SQRT3, .y = INV_SQRT3, .z = INV_SQRT3};


void vertex_shader(const Vertex *in, const Uniforms *u, VertexShaderOutput *out) {
    out->clip = glms_mat4_mulv(u->mvp, glms_vec4(in->pos, 1.0f));
    out->v.normal = in->normal;
}

uint32_t fragment_shader(const Varyings *v) {

    vec3s normal = glms_vec3_normalize(v->normal);

    // Lambert diffuse
    float intensity = fmaxf(glms_vec3_dot(normal, light_dir), 0.0f);
    vec3s color = glms_vec3_broadcast(intensity * 255.0f);

    return rgba(color);
}

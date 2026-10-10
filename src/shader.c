#include "shader.h"

#include <math.h>
#include "util.h"

// Normalized nit vector towards the light, (1, 1, 1)
#define INV_SQRT3 0.57735027f
static const vec3s light_dir = {.x = INV_SQRT3, .y = INV_SQRT3, .z = INV_SQRT3};


void varyings_interpolate(const Varyings *v0, const Varyings *v1, const Varyings *v2, vec3s weights, Varyings *out) {
    out->normal = weighted_sum_vec3(v0->normal, v1->normal, v2->normal, weights);
    out->uv = weighted_sum_vec2(v0->uv, v1->uv, v2->uv, weights);
}

void vertex_shader(const Vertex *in, const Uniforms *u, VertexShaderOutput *out) {
    out->clip = glms_mat4_mulv(u->mvp, glms_vec4(in->pos, 1.0f));
    out->v.normal = in->normal;
    out->v.uv = in->uv;
}

uint32_t fragment_shader(const Varyings *v, const Material *material) {

    vec3s normal = glms_vec3_normalize(v->normal);

    bool has_texture = material->diffuse_texture.texels != NULL;
    vec3s albedo = has_texture ? texture_sample(&material->diffuse_texture, v->uv) : material->diffuse;

    // Lambert diffuse
    float intensity = fmaxf(glms_vec3_dot(normal, light_dir), 0.0f);
    vec3s color = glms_vec3_scale(albedo, intensity * 255.0f);

    return rgba(color);
}

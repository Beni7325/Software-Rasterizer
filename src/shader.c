#include "shader.h"

#include "framebuffer.h"


void vertex_shader(const Vertex *in, const Uniforms *u, VertexShaderOutput *out) {
    out->clip = glms_mat4_mulv(u->mvp, glms_vec4(in->pos, 1.0f));
    out->v.color = in->color;
}

uint32_t fragment_shader(const Varyings *v) {
    return rgba(v->color);
}

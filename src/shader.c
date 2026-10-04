#include "shader.h"

#include "framebuffer.h"


void vertex_shader(const Vertex *in, VertexShaderOutput *out) {
    out->clip = glms_vec4(in->pos, 1.0f);
    out->v.color = in->color;
}

uint32_t fragment_shader(const Varyings *v) {
    return rgba(v->color);
}

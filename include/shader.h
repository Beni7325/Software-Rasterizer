#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>
#include <cglm/struct.h>
#include "mesh.h"


typedef struct {
    vec3s normal;
    vec2s uv;
} Varyings;

typedef struct {
    vec4s clip;
    vec4s screen;
    Varyings v;
} VertexShaderOutput;

typedef struct {
    mat4s mvp;
} Uniforms;

void vertex_shader(const Vertex *in, const Uniforms *u, VertexShaderOutput *out);
uint32_t fragment_shader(const Varyings *v, const Material *material);
void varyings_interpolate(const Varyings *v0, const Varyings *v1, const Varyings *v2, vec3s weights, Varyings *out);

#endif /* SHADER_H */

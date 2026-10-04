#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>
#include <cglm/struct.h>
#include "mesh.h"


typedef struct {
    vec3s color;
} Varyings;

typedef struct {
    vec4s clip;
    Varyings v; 
} VertexShaderOutput;

void vertex_shader(const Vertex *in, VertexShaderOutput *out);
uint32_t fragment_shader(const Varyings *v);

#endif /* SHADER_H */

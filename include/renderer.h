#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "framebuffer.h"
#include "mesh.h"
#include "model.h"
#include "shader.h"


void draw_mesh(FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms);
void draw_model(FrameBuffer *fb, const Model *model, const Camera *camera);

#endif /* RENDERER_H */

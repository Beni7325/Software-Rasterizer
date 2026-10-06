#ifndef MODEL_H
#define MODEL_H

#include <cglm/struct.h>
#include "mesh.h"


typedef struct {
    const Mesh *mesh;
    vec3s position;
    vec3s rotation; // Euler angles in radians
    vec3s scale;
} Model;

Model model_create(const Mesh *mesh);
mat4s model_matrix(const Model *model);

void model_translate(Model *model, vec3s offset);
void model_rotate(Model *model, vec3s angles);
void model_scale(Model *model, vec3s factors);

#endif /* MODEL_H */

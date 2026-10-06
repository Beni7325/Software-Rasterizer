#include "model.h"


Model model_create(const Mesh *mesh) {
    return (Model){
        .mesh = mesh,
        .position = glms_vec3_zero(),
        .rotation = glms_vec3_zero(),
        .scale = glms_vec3_one(),
    };
}

mat4s model_matrix(const Model *model) {

    mat4s translation = glms_translate_make(model->position);
    mat4s rotation = glms_euler_xyz(model->rotation);
    mat4s scale = glms_scale_make(model->scale);

    mat4s translation_rotation = glms_mat4_mul(translation, rotation);

    return glms_mat4_mul(translation_rotation, scale);
}

void model_translate(Model *model, vec3s offset) {
    model->position = glms_vec3_add(model->position, offset);
}

void model_rotate(Model *model, vec3s angles) {
    model->rotation = glms_vec3_add(model->rotation, angles);
}

void model_scale(Model *model, vec3s factors) {
    model->scale = glms_vec3_mul(model->scale, factors);
}

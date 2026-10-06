#include "camera.h"

#include <math.h>

// < 90 degrees, looking straight up or down breaks the view matrix (dir parallel to up)
#define MAX_PITCH glm_rad(89.0f)


static void update_view_projection(Camera *camera) {
    camera->view_projection = glms_mat4_mul(camera->projection, camera->view);
}

static void update_dir(Camera *camera) {

    float cos_pitch = cosf(camera->pitch);

    camera->dir = (vec3s){
        .x = cos_pitch * sinf(camera->yaw),
        .y = sinf(camera->pitch),
        .z = -cos_pitch * cosf(camera->yaw),
    };
}

Camera camera_init(vec3s pos, vec3s dir, float fov_y, float aspect) {

    vec3s dir_n = glms_vec3_normalize(dir);
    float pitch = asinf(dir_n.y);

    Camera camera = {
        .pos = pos,
        .up = {.x = 0.0f, .y = 1.0f, .z = 0.0f},
        .yaw = atan2f(dir_n.x, -dir_n.z),
        .pitch = glm_clamp(pitch, -MAX_PITCH, MAX_PITCH),
        .fov_y = fov_y,
        .aspect = aspect,
        .z_near = 0.1f,
        .z_far = 100.0f,
    };

    update_dir(&camera);
    camera_update_view(&camera);
    camera_update_projection(&camera);

    return camera;
}

void camera_update_view(Camera *camera) {
    camera->view = glms_look(camera->pos, camera->dir, camera->up);
    update_view_projection(camera);
}

void camera_update_projection(Camera *camera) {
    camera->projection = glms_perspective(camera->fov_y, camera->aspect, camera->z_near, camera->z_far);
    update_view_projection(camera);
}

void camera_move(Camera *camera, vec3s offset) {
    camera->pos = glms_vec3_add(camera->pos, offset);
}

void camera_rotate(Camera *camera, float yaw_delta, float pitch_delta) {

    float yaw = camera->yaw + yaw_delta;
    float pitch = camera->pitch + pitch_delta;

    // Wrap to [-pi, pi] so precision doesn't degrade after many turns
    camera->yaw = remainderf(yaw, 2.0f * GLM_PIf);
    camera->pitch = glm_clamp(pitch, -MAX_PITCH, MAX_PITCH);

    update_dir(camera);
}

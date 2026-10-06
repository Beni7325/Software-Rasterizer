#include "camera_controller.h"

#include <stdbool.h>
#include <SDL2/SDL.h>

#define CAMERA_SPEED 2.0f
#define MOUSE_SENSITIVITY 0.0025f


void camera_controller_update(Camera *camera, float dt, vec2s mouse_delta) {

    bool changed = false;

    // Screen y grows downwards, pitch grows upwards
    if (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) {
        camera_rotate(camera, mouse_delta.x * MOUSE_SENSITIVITY, -mouse_delta.y * MOUSE_SENSITIVITY);
        changed = true;
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    vec3s flat_dir = {.x = camera->dir.x, .y = 0.0f, .z = camera->dir.z};
    vec3s forward = glms_vec3_normalize(flat_dir);
    vec3s right = glms_vec3_crossn(forward, camera->up);

    vec3s move = glms_vec3_zero();
    if (keys[SDL_SCANCODE_W]) move = glms_vec3_add(move, forward);
    if (keys[SDL_SCANCODE_S]) move = glms_vec3_sub(move, forward);
    if (keys[SDL_SCANCODE_D]) move = glms_vec3_add(move, right);
    if (keys[SDL_SCANCODE_A]) move = glms_vec3_sub(move, right);
    if (keys[SDL_SCANCODE_SPACE]) move = glms_vec3_add(move, camera->up);
    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) move = glms_vec3_sub(move, camera->up);

    // Skipped when no keys are held or opposite keys cancel out
    if (glms_vec3_norm2(move) != 0.0f) {

        // Normalize so diagonals aren't faster
        vec3s move_dir = glms_vec3_normalize(move);
        vec3s offset = glms_vec3_scale(move_dir, CAMERA_SPEED * dt);

        camera_move(camera, offset);
        changed = true;
    }

    if (changed) {
        camera_update_view(camera);
    }
}

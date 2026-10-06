#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/struct.h>


typedef struct {
    vec3s pos;
    vec3s dir;    // Normalized, derived from yaw and pitch
    vec3s up;
    float yaw;    // Radians, 0 looks down -Z, positive turns right
    float pitch;  // Radians, positive looks up
    float fov_y;  // Vertical, in radians
    float aspect;
    float z_near;
    float z_far;

    // Cached, call the matching update function after changing the fields above
    mat4s view;
    mat4s projection;
    mat4s view_projection;
} Camera;

Camera camera_init(vec3s pos, vec3s dir, float fov_y, float aspect);
void camera_update_view(Camera *camera);
void camera_update_projection(Camera *camera);

// These only change the fields, call camera_update_view afterwards
void camera_move(Camera *camera, vec3s offset);
void camera_rotate(Camera *camera, float yaw_delta, float pitch_delta);

#endif /* CAMERA_H */

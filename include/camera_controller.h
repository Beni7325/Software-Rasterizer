#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <cglm/struct.h>
#include "camera.h"


void camera_controller_update(Camera *camera, float dt, vec2s mouse_delta);

#endif /* CAMERA_CONTROLLER_H */

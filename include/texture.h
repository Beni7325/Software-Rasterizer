#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdbool.h>
#include <stdint.h>

#include <cglm/struct.h>


typedef struct {
    int width;
    int height;
    uint8_t *texels;
} Texture;

bool texture_load(Texture *texture, const char *path);
void texture_free(Texture *texture);
vec3s texture_sample(const Texture *texture, vec2s uv);

#endif /* TEXTURE_H */

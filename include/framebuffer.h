#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cglm/struct.h>


typedef struct {
    size_t width;
    size_t height;
    uint32_t *color_buffer;
    float *depth_buffer;
} FrameBuffer;

bool framebuffer_init(FrameBuffer *fb, size_t width, size_t height);
void framebuffer_free(FrameBuffer *fb);
void framebuffer_clear(FrameBuffer *fb);

static inline uint32_t rgba(vec3s color) {
    return ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) | ((uint32_t)color.b << 8) | 255;
}

#endif /* FRAMEBUFFER_H */

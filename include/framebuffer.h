#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct {
    size_t width;
    size_t height;
    uint32_t *color_buffer;
    float *depth_buffer;
} FrameBuffer;

bool framebuffer_init(FrameBuffer *fb, size_t width, size_t height);
void framebuffer_free(FrameBuffer *fb);
void framebuffer_clear(FrameBuffer *fb);

#endif /* FRAMEBUFFER_H */

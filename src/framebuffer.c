#include "framebuffer.h"

#include <stdlib.h>
#include <string.h>


bool framebuffer_init(FrameBuffer *fb, size_t width, size_t height) {

    if (!fb || width == 0 || height == 0) {
        return false;
    }

    size_t pixel_count = width * height;
    fb->color_buffer = malloc(sizeof(*fb->color_buffer) * pixel_count);
    fb->depth_buffer = malloc(sizeof(*fb->depth_buffer) * pixel_count);

    if (!fb->color_buffer || !fb->depth_buffer) {
        free(fb->color_buffer);
        free(fb->depth_buffer);
        fb->color_buffer = NULL;
        fb->depth_buffer = NULL;

        return false;
    }

    fb->width = width;
    fb->height = height;

    return true;
}

void framebuffer_free(FrameBuffer *fb) {

    if (!fb) {
        return;
    }

    free(fb->color_buffer);
    free(fb->depth_buffer);
    fb->color_buffer = NULL;
    fb->depth_buffer = NULL;
}

void framebuffer_clear(FrameBuffer *fb) {
    memset(fb->color_buffer, 0, fb->width * fb->height * sizeof(uint32_t));
}

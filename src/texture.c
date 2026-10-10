#include "texture.h"

#include <stdio.h>
#include "util.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


bool texture_load(Texture *texture, const char *path) {

    *texture = (Texture){0};

    int channels;
    texture->texels = stbi_load(path, &texture->width, &texture->height, &channels, 4);
    if (!texture->texels) {
        fprintf(stderr, "Failed to load texture '%s': %s\n", path, stbi_failure_reason());
        return false;
    }

    return true;
}

void texture_free(Texture *texture) {

    stbi_image_free(texture->texels);
    *texture = (Texture){0};
}

// Nearest neighbour lookup with repeat wrapping, uv (0, 0) is the bottom left corner. Returns RGB in [0, 1]
vec3s texture_sample(const Texture *texture, vec2s uv) {

    // Keep only the fractional part so coordinates outside [0, 1] repeat
    vec2s wrapped = fract_vec2(uv);

    // v grows upwards but image rows go down
    int x = wrapped.x * texture->width;
    int y = (1.0f - wrapped.y) * texture->height;

    // u or v exactly 0 can land one past the edge
    x = x < texture->width ? x : texture->width - 1;
    y = y < texture->height ? y : texture->height - 1;

    const uint8_t *texel = &texture->texels[4 * ((size_t)y * texture->width + x)];
    vec3s color = {.r = texel[0], .g = texel[1], .b = texel[2]};

    return glms_vec3_scale(color, 1.0f / 255.0f);
}

#include <SDL2/SDL.h>
#include <cglm/struct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH  256
#define HEIGHT 256


static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a;
}

static void fill_solid(uint32_t *pixels, uint32_t color) {
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        pixels[i] = color;
    }
}

float edge(vec2s a, vec2s b, vec2s p) {

    vec2s ab = glms_vec2_sub(b, a);
    vec2s ap = glms_vec2_sub(p, a);

    return glms_vec2_cross(ab, ap);
}

// TODO: Implement top-left rule
void rasterize_triangle(uint32_t *framebuffer, vec2s a, vec2s b, vec2s c, vec3s color_a, vec3s color_b, vec3s color_c) {

    float double_area = edge(a, b, c);
    if (double_area == 0) {
        return;
    } else if (double_area < 0) {
        vec2s tmp_v = b; b = c; c = tmp_v;
        vec3s tmp_c = color_b; color_b = color_c; color_c = tmp_c;
        double_area = -double_area;
    }

    mat3s colors = {.col = {color_a, color_b, color_c}};

    // Triangle bounding box
    vec2s bb_min = glms_vec2_minv(a, glms_vec2_minv(b, c));
    vec2s bb_max = glms_vec2_maxv(a, glms_vec2_maxv(b, c));

    // Clamp to the framebuffer
    vec2s screen_min = {.x = 0, .y = 0 };
    vec2s screen_max = {.x = WIDTH - 1.0f, .y = HEIGHT - 1.0f};

    bb_min = glms_vec2_maxv(bb_min, screen_min);
    bb_max = glms_vec2_minv(bb_max, screen_max);

    // Pixel range covered by the box
    // TODO: Tighten to pixel centers
    ivec2s px_min = {.x = (int)floorf(bb_min.x), .y = (int)floorf(bb_min.y)};
    ivec2s px_max = {.x = (int)ceilf(bb_max.x),  .y = (int)ceilf(bb_max.y)};

    for (int y = px_min.y; y <= px_max.y; ++y) {
        for (int x = px_min.x; x <= px_max.x; ++x) {

            // Sample at pixel center
            vec2s p = {.x = x + 0.5f, .y = y + 0.5f};
            vec3s w = {
                .x = edge(b, c, p),
                .y = edge(c, a, p),
                .z = edge(a, b, p),
            };

            if (w.x >= 0 && w.y >= 0 && w.z >= 0) {

                // Interpolate
                vec3s l = glms_vec3_divs(w, double_area);
                vec3s color = glms_mat3_mulv(colors, l);

                framebuffer[y * WIDTH + x] = rgba((uint8_t)color.r, (uint8_t)color.g, (uint8_t)color.b, 255);
            }
        }
    }
}

int main(void) {

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("rasterizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT) : NULL;
    uint32_t *framebuffer = malloc(sizeof(uint32_t) * WIDTH * HEIGHT);
    if (!texture || !framebuffer) {
        fprintf(stderr, "setup failed: %s\n", SDL_GetError());
        return 1;
    }

    vec2s a = {.x = 128.0f, .y =  56.0f};
    vec2s b = {.x =  32.0f, .y = 224.0f};
    vec2s c = {.x = 224.0f, .y = 224.0f};
    vec3s color_a = {.r = 255, .g =   0, .b =   0};
    vec3s color_b = {.r =   0, .g = 255, .b =   0};
    vec3s color_c = {.r =   0, .g =   0, .b = 255};

    const uint32_t sky = rgba(135, 206, 235, 255);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        fill_solid(framebuffer, sky);
        rasterize_triangle(framebuffer, a, b, c, color_a, color_b, color_c);

        SDL_UpdateTexture(texture, NULL, framebuffer, WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    free(framebuffer);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

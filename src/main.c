#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "framebuffer.h"
#include "mesh.h"
#include "renderer.h"

#define WIDTH  256
#define HEIGHT 256


int main(void) {

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("rasterizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT) : NULL;
    if (!texture) {
        fprintf(stderr, "setup failed: %s\n", SDL_GetError());
        return 1;
    }

    FrameBuffer fb = {0};
    if (!framebuffer_init(&fb, WIDTH, HEIGHT)) {
        fprintf(stderr, "Error initializing the framebuffer: %s\n", SDL_GetError());
        // TODO: Free SDL
        return 1;
    }

    // Smaller z = closer to the camera
    Vertex vertices[] = {
        // Solid white, in front of the gradient triangle but submitted before it
        {.pos = {.x =  96.0f, .y =  96.0f, .z = 0.25f}, .color = {.r = 255, .g = 255, .b = 255}},
        {.pos = {.x =  48.0f, .y = 224.0f, .z = 0.25f}, .color = {.r = 255, .g = 255, .b = 255}},
        {.pos = {.x = 160.0f, .y = 224.0f, .z = 0.25f}, .color = {.r = 255, .g = 255, .b = 255}},

        // Gradient
        {.pos = {.x =  72.0f, .y =  32.0f, .z = 0.5f}, .color = {.r = 255, .g =   0, .b =   0}},
        {.pos = {.x =  16.0f, .y = 160.0f, .z = 0.5f}, .color = {.r =   0, .g = 255, .b =   0}},
        {.pos = {.x = 128.0f, .y = 160.0f, .z = 0.5f}, .color = {.r =   0, .g =   0, .b = 255}},

        // Solid orange, no overlap with the others
        {.pos = {.x = 200.0f, .y =  32.0f, .z = 0.5f}, .color = {.r = 255, .g = 128, .b =   0}},
        {.pos = {.x = 160.0f, .y = 112.0f, .z = 0.5f}, .color = {.r = 255, .g = 128, .b =   0}},
        {.pos = {.x = 240.0f, .y = 112.0f, .z = 0.5f}, .color = {.r = 255, .g = 128, .b =   0}},
    };
    Mesh scene = {
        .vertex_count = sizeof(vertices) / sizeof(vertices[0]),
        .vertex_buffer = vertices,
    };

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        framebuffer_clear(&fb);
        draw_call(&fb, &scene);

        SDL_UpdateTexture(texture, NULL, fb.color_buffer, WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    framebuffer_free(&fb);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

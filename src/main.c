#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "camera.h"
#include "camera_controller.h"
#include "framebuffer.h"
#include "mesh.h"
#include "model.h"
#include "renderer.h"

#define WIDTH  1280
#define HEIGHT 720


void sdl_free(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture) {

    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <model.obj>\n", argv[0]);
        return 1;
    }

    Mesh mesh;
    if (!mesh_load_obj(&mesh, argv[1])) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        mesh_free(&mesh);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("rasterizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT) : NULL;
    if (!texture) {
        fprintf(stderr, "setup failed: %s\n", SDL_GetError());
        sdl_free(window, renderer, texture);
        mesh_free(&mesh);
        return 1;
    }

    FrameBuffer fb = {0};
    if (!framebuffer_init(&fb, WIDTH, HEIGHT)) {
        fprintf(stderr, "Error initializing the framebuffer: %s\n", SDL_GetError());
        sdl_free(window, renderer, texture);
        mesh_free(&mesh);
        return 1;
    }

    Model model = model_create(&mesh);

    vec3s camera_pos = {.x = 0.0f, .y = 0.0f, .z = 3.0f};
    vec3s camera_dir = {.x = 0.0f, .y = 0.0f, .z = -1.0f};
    float aspect = (float)WIDTH / HEIGHT;
    Camera camera = camera_init(camera_pos, camera_dir, glm_rad(60.0f), aspect);

    Uint64 last_counter = SDL_GetPerformanceCounter();

    int running = 1;
    while (running) {

        // Several motion events can arrive per frame so we sum them
        vec2s mouse_delta = glms_vec2_zero();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            } else if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_LMASK)) {  // Drag to look
                mouse_delta.x += e.motion.xrel;
                mouse_delta.y += e.motion.yrel;
            }
        }

        Uint64 counter = SDL_GetPerformanceCounter();
        float dt = (float)(counter - last_counter) / SDL_GetPerformanceFrequency();
        last_counter = counter;

        camera_controller_update(&camera, dt, mouse_delta);

        framebuffer_clear(&fb);
        draw_model(&fb, &model, &camera);

        SDL_UpdateTexture(texture, NULL, fb.color_buffer, WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    framebuffer_free(&fb);
    sdl_free(window, renderer, texture);
    mesh_free(&mesh);

    return 0;
}

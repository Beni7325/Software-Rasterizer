#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "camera.h"
#include "camera_controller.h"
#include "framebuffer.h"
#include "mesh.h"
#include "model.h"
#include "renderer.h"
#include "tile.h"

#define WIDTH  1280
#define HEIGHT 720

// Seconds between frame time updates in the window title
#define STATS_INTERVAL 0.5f


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

    // Round up to whole tiles so no tile hangs off the edge of the framebuffer
    int width = (WIDTH + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE;
    int height = (HEIGHT + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        mesh_free(&mesh);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("rasterizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height) : NULL;
    if (!texture) {
        fprintf(stderr, "setup failed: %s\n", SDL_GetError());
        sdl_free(window, renderer, texture);
        mesh_free(&mesh);
        return 1;
    }

    FrameBuffer fb = {0};
    if (!framebuffer_init(&fb, width, height)) {
        fprintf(stderr, "Error initializing the framebuffer: %s\n", SDL_GetError());
        sdl_free(window, renderer, texture);
        mesh_free(&mesh);
        return 1;
    }

    ThreadPool pool;
    if (!thread_pool_init(&pool, SDL_GetCPUCount())) {
        fprintf(stderr, "Error initializing the thread pool\n");
        exit(1);
    }

    Model model = model_create(&mesh);

    // Back off along +z until the mesh's bounding cube fits the vertical fov, which is the narrower one for a wide window
    float fov_y = glm_rad(60.0f);
    float half_size = 0.5f * NORMALIZED_MESH_SIZE;
    float fit_distance = half_size / tanf(0.5f * fov_y);
    float camera_z = half_size + fit_distance;

    vec3s camera_pos = {.x = 0.0f, .y = 0.0f, .z = camera_z};
    vec3s camera_dir = {.x = 0.0f, .y = 0.0f, .z = -1.0f};
    float aspect = (float)width / height;
    Camera camera = camera_init(camera_pos, camera_dir, fov_y, aspect);

    Uint64 last_counter = SDL_GetPerformanceCounter();

    // Frame timings are averaged over a short window and shown in the title
    float counter_period = 1.0f / SDL_GetPerformanceFrequency();
    float stats_elapsed = 0.0f;
    float stats_render_time = 0.0f;
    int stats_frames = 0;

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
        float dt = (float)(counter - last_counter) * counter_period;
        last_counter = counter;

        camera_controller_update(&camera, dt, mouse_delta);

        // Render time excludes the vsync wait
        Uint64 render_start = SDL_GetPerformanceCounter();
        framebuffer_clear(&fb);
        draw_model(&fb, &model, &camera, &pool);
        Uint64 render_end = SDL_GetPerformanceCounter();
        float render_time = (float)(render_end - render_start) * counter_period;

        stats_elapsed += dt;
        stats_render_time += render_time;
        stats_frames++;

        if (stats_elapsed >= STATS_INTERVAL) {
            float frame_ms = 1000.0f * stats_elapsed / stats_frames;
            float render_ms = 1000.0f * stats_render_time / stats_frames;

            char title[128];
            snprintf(title, sizeof(title), "rasterizer | frame %.2f ms | render %.2f ms", frame_ms, render_ms);
            SDL_SetWindowTitle(window, title);

            stats_elapsed = 0.0f;
            stats_render_time = 0.0f;
            stats_frames = 0;
        }

        SDL_UpdateTexture(texture, NULL, fb.color_buffer, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    thread_pool_free(&pool);
    framebuffer_free(&fb);
    sdl_free(window, renderer, texture);
    mesh_free(&mesh);

    return 0;
}

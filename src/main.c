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
        sdl_free(window, renderer, texture);
        return 1;
    }

    FrameBuffer fb = {0};
    if (!framebuffer_init(&fb, WIDTH, HEIGHT)) {
        fprintf(stderr, "Error initializing the framebuffer: %s\n", SDL_GetError());
        sdl_free(window, renderer, texture);
        return 1;
    }

    const vec3s lbf = {.x = -0.5f, .y = -0.5f, .z =  0.5f};
    const vec3s rbf = {.x =  0.5f, .y = -0.5f, .z =  0.5f};
    const vec3s rtf = {.x =  0.5f, .y =  0.5f, .z =  0.5f};
    const vec3s ltf = {.x = -0.5f, .y =  0.5f, .z =  0.5f};
    const vec3s lbb = {.x = -0.5f, .y = -0.5f, .z = -0.5f};
    const vec3s rbb = {.x =  0.5f, .y = -0.5f, .z = -0.5f};
    const vec3s rtb = {.x =  0.5f, .y =  0.5f, .z = -0.5f};
    const vec3s ltb = {.x = -0.5f, .y =  0.5f, .z = -0.5f};

    const vec3s black   = {.r =   0, .g =   0, .b =   0};
    const vec3s red     = {.r = 255, .g =   0, .b =   0};
    const vec3s green   = {.r =   0, .g = 255, .b =   0};
    const vec3s blue    = {.r =   0, .g =   0, .b = 255};
    const vec3s yellow  = {.r = 255, .g = 255, .b =   0};
    const vec3s magenta = {.r = 255, .g =   0, .b = 255};
    const vec3s cyan    = {.r =   0, .g = 255, .b = 255};
    const vec3s white   = {.r = 255, .g = 255, .b = 255};

    // One vertex per corner, colored as an RGB cube: x -> red, y -> green, z -> blue.
    // Corners are shared between faces, so colors blend across each face
    enum { LBB, RBB, LTB, RTB, LBF, RBF, LTF, RTF };

    const Vertex vertices[] = {
        [LBB] = {.pos = lbb, .color = black},
        [RBB] = {.pos = rbb, .color = red},
        [LTB] = {.pos = ltb, .color = green},
        [RTB] = {.pos = rtb, .color = yellow},
        [LBF] = {.pos = lbf, .color = blue},
        [RBF] = {.pos = rbf, .color = magenta},
        [LTF] = {.pos = ltf, .color = cyan},
        [RTF] = {.pos = rtf, .color = white},
    };

    // Counter clockwise when seen from outside the cube
    const uint32_t indices[] = {
        // Front (+z)
        LBF, RBF, RTF,   LBF, RTF, LTF,
        // Back (-z)
        RBB, LBB, LTB,   RBB, LTB, RTB,
        // Right (+x)
        RBF, RBB, RTB,   RBF, RTB, RTF,
        // Left (-x)
        LBB, LBF, LTF,   LBB, LTF, LTB,
        // Top (+y)
        LTF, RTF, RTB,   LTF, RTB, LTB,
        // Bottom (-y)
        LBB, RBB, RBF,   LBB, RBF, LBF,
    };

    Mesh scene = {
        .vertex_count = sizeof(vertices) / sizeof(vertices[0]),
        .index_count = sizeof(indices) / sizeof(indices[0]),
        .vertex_buffer = vertices,
        .index_buffer = indices,
    };
    if (!mesh_validate(&scene)) {
        framebuffer_free(&fb);
        sdl_free(window, renderer, texture);
        return 1;
    }

    Model model = model_create(&scene);
    vec3s model_rotation = {.y = glm_rad(45.0f)};
    model_rotate(&model, model_rotation);

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

    return 0;
}

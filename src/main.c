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

    const vec3s lbf = {.x = -0.5f, .y = -0.5f, .z =  0.5f};
    const vec3s rbf = {.x =  0.5f, .y = -0.5f, .z =  0.5f};
    const vec3s rtf = {.x =  0.5f, .y =  0.5f, .z =  0.5f};
    const vec3s ltf = {.x = -0.5f, .y =  0.5f, .z =  0.5f};
    const vec3s lbb = {.x = -0.5f, .y = -0.5f, .z = -0.5f};
    const vec3s rbb = {.x =  0.5f, .y = -0.5f, .z = -0.5f};
    const vec3s rtb = {.x =  0.5f, .y =  0.5f, .z = -0.5f};
    const vec3s ltb = {.x = -0.5f, .y =  0.5f, .z = -0.5f};

    const vec3s red     = {.r = 255, .g =   0, .b =   0};
    const vec3s green   = {.r =   0, .g = 255, .b =   0};
    const vec3s blue    = {.r =   0, .g =   0, .b = 255};
    const vec3s yellow  = {.r = 255, .g = 255, .b =   0};
    const vec3s magenta = {.r = 255, .g =   0, .b = 255};
    const vec3s cyan    = {.r =   0, .g = 255, .b = 255};

    Vertex vertices[] = {
        // Front (+z)
        {.pos = lbf, .color = red},     {.pos = rbf, .color = red},     {.pos = rtf, .color = red},
        {.pos = lbf, .color = red},     {.pos = rtf, .color = red},     {.pos = ltf, .color = red},
        // Back (-z)
        {.pos = rbb, .color = green},   {.pos = lbb, .color = green},   {.pos = ltb, .color = green},
        {.pos = rbb, .color = green},   {.pos = ltb, .color = green},   {.pos = rtb, .color = green},
        // Right (+x)
        {.pos = rbf, .color = blue},    {.pos = rbb, .color = blue},    {.pos = rtb, .color = blue},
        {.pos = rbf, .color = blue},    {.pos = rtb, .color = blue},    {.pos = rtf, .color = blue},
        // Left (-x)
        {.pos = lbb, .color = yellow},  {.pos = lbf, .color = yellow},  {.pos = ltf, .color = yellow},
        {.pos = lbb, .color = yellow},  {.pos = ltf, .color = yellow},  {.pos = ltb, .color = yellow},
        // Top (+y)
        {.pos = ltf, .color = magenta}, {.pos = rtf, .color = magenta}, {.pos = rtb, .color = magenta},
        {.pos = ltf, .color = magenta}, {.pos = rtb, .color = magenta}, {.pos = ltb, .color = magenta},
        // Bottom (-y)
        {.pos = lbb, .color = cyan},    {.pos = rbb, .color = cyan},    {.pos = rbf, .color = cyan},
        {.pos = lbb, .color = cyan},    {.pos = rbf, .color = cyan},    {.pos = lbf, .color = cyan},
    };
    Mesh scene = {
        .vertex_count = sizeof(vertices) / sizeof(vertices[0]),
        .vertex_buffer = vertices,
    };

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
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

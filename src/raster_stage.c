#include "pipeline.h"

#include <math.h>


typedef struct {
    // On its own cache line, to avoid false sharing when threads update the atomic counter
    _Alignas(CACHE_LINE_SIZE) atomic_int next_tile;

    _Alignas(CACHE_LINE_SIZE) FrameBuffer *fb;
    const Mesh *mesh;
    const VertexShaderOutput *v_out;
    const TriangleStageContext *contexts;
    int context_count;
} RasterStage;

// TODO: Implement top-left rule
static void rasterize_triangle(FrameBuffer *fb, const VertexShaderOutput *v0, const VertexShaderOutput *v1, const VertexShaderOutput *v2, const Material *material, ivec2s tile_min, ivec2s tile_max) {

    vec2s a = screen_xy(v0);
    vec2s b = screen_xy(v1);
    vec2s c = screen_xy(v2);

    float double_area = edge_function(a, b, c);
    float inv_double_area = 1.0f / double_area;

    vec3s depths = {.x = v0->screen.z, .y = v1->screen.z, .z = v2->screen.z};
    vec3s inv_ws = {.x = v0->screen.w, .y = v1->screen.w, .z = v2->screen.w};

    // Triangle bounding box
    vec2s bb_min = min3_vec2(a, b, c);
    vec2s bb_max = max3_vec2(a, b, c);

    // Pixels whose center (x + 0.5, y + 0.5) lies inside the box
    vec2s half_pixel = {.x = 0.5f, .y = 0.5f};
    vec2s centers_min = glms_vec2_sub(bb_min, half_pixel);
    vec2s centers_max = glms_vec2_sub(bb_max, half_pixel);

    ivec2s px_min = {.x = (int)ceilf(centers_min.x),  .y = (int)ceilf(centers_min.y)};
    ivec2s px_max = {.x = (int)floorf(centers_max.x), .y = (int)floorf(centers_max.y)};

    // Clamp to the framebuffer
    ivec2s screen_min = {.x = 0, .y = 0};
    ivec2s screen_max = {.x = fb->width - 1, .y = fb->height - 1};

    px_min = glms_ivec2_maxv(px_min, screen_min);
    px_max = glms_ivec2_minv(px_max, screen_max);

    // Clamp to the tile being rasterized
    px_min = glms_ivec2_maxv(px_min, tile_min);
    px_max = glms_ivec2_minv(px_max, tile_max);

    // Edge functions are linear: evaluate them at the top left pixel center, then step per pixel in x and y
    vec2s p_tl = {.x = px_min.x + 0.5f, .y = px_min.y + 0.5f};
    vec3s w_tl = {.x = edge_function(b, c, p_tl), .y = edge_function(c, a, p_tl), .z = edge_function(a, b, p_tl)};
    vec3s w_dx = {.x = b.y - c.y, .y = c.y - a.y, .z = a.y - b.y};
    vec3s w_dy = {.x = c.x - b.x, .y = a.x - c.x, .z = b.x - a.x};

    Varyings interpolated_varyings;

    for (int y = px_min.y; y <= px_max.y; ++y) {

        vec3s row_offset = glms_vec3_scale(w_dy, y - px_min.y);
        vec3s w_row = glms_vec3_add(w_tl, row_offset);

        for (int x = px_min.x; x <= px_max.x; ++x) {

            vec3s col_offset = glms_vec3_scale(w_dx, x - px_min.x);
            vec3s w = glms_vec3_add(w_row, col_offset);

            if (w.x >= 0 && w.y >= 0 && w.z >= 0) {

                // Interpolate depth (NDC depth is affine in screen space)
                vec3s l = glms_vec3_scale(w, inv_double_area);
                float depth = glms_vec3_dot(depths, l);

                int idx = y * fb->width + x;
                if (depth < fb->depth_buffer[idx]) {
                    fb->depth_buffer[idx] = depth;

                    // Perspective-correct interpolation of varyings
                    vec3s l_over_w = glms_vec3_mul(l, inv_ws);
                    float inv_w = glms_vec3_hadd(l_over_w);
                    vec3s l_persp = glms_vec3_divs(l_over_w, inv_w);

                    varyings_interpolate(&v0->v, &v1->v, &v2->v, l_persp, &interpolated_varyings);
                    fb->color_buffer[idx] = fragment_shader(&interpolated_varyings, material);
                }
            }
        }
    }
}

static void rasterize_tile(const RasterStage *stage, int tile) {

    const Mesh *mesh = stage->mesh;
    size_t mesh_triangle_count = mesh->index_count / 3;

    // Every context's bins cover the same tile grid
    ivec2s tiles = stage->contexts[0].tile_bins.tiles;
    int tx = tile % tiles.x;
    int ty = tile / tiles.x;

    ivec2s tile_min = {.x = tx * TILE_SIZE, .y = ty * TILE_SIZE};
    ivec2s tile_max = {.x = tile_min.x + TILE_SIZE - 1, .y = tile_min.y + TILE_SIZE - 1};

    // The tile's triangles are spread over the bins of every thread that binned some of them
    for (int i = 0; i < stage->context_count; ++i) {

        const TriangleStageContext *tc = &stage->contexts[i];

        for (const BinChunk *chunk = tc->tile_bins.bins[tile]; chunk; chunk = chunk->next) {
            for (uint32_t k = 0; k < chunk->count; ++k) {

                uint32_t triangle_id = chunk->triangle_ids[k];

                // Past the mesh's triangles the indices and vertices are in the context's clip buffers
                const uint32_t *indices;
                const VertexShaderOutput *vertices;
                uint32_t material_id;
                if (triangle_id < mesh_triangle_count) {
                    indices = &mesh->index_buffer[3 * triangle_id];
                    vertices = stage->v_out;
                    material_id = mesh->material_ids[triangle_id];
                } else {
                    uint32_t clip_triangle = triangle_id - mesh_triangle_count;
                    indices = &tc->clip_index_buffer[3 * clip_triangle];
                    vertices = tc->clip_v_out;
                    material_id = tc->clip_material_ids[clip_triangle];
                }

                const VertexShaderOutput *v0, *v1, *v2;
                fetch_triangle(vertices, indices, &v0, &v1, &v2);
                const Material *material = &mesh->materials[material_id];

                rasterize_triangle(stage->fb, v0, v1, v2, material, tile_min, tile_max);
            }
        }
    }
}

// A tile row has to cover whole cache lines in the cache line aligned color and depth buffers,
// otherwise threads rasterizing neighbouring tiles would write to the same line
_Static_assert(TILE_SIZE * sizeof(uint32_t) % CACHE_LINE_SIZE == 0, "Tile rows must be a multiple of the cache line size");
_Static_assert(TILE_SIZE * sizeof(float) % CACHE_LINE_SIZE == 0, "Tile rows must be a multiple of the cache line size");

// Threads take one tile at a time. Tiles don't overlap, so no two threads write the same pixel
static void raster_stage_task(void *ctx) {

    RasterStage *stage = ctx;
    ivec2s tiles = stage->contexts[0].tile_bins.tiles;
    int tile_count = tiles.x * tiles.y;

    for (;;) {
        int tile = atomic_fetch_add_explicit(&stage->next_tile, 1, memory_order_relaxed);
        if (tile >= tile_count) {
            break;
        }

        rasterize_tile(stage, tile);
    }
}

void raster_stage_run(ThreadPool *pool, FrameBuffer *fb, const Mesh *mesh, const VertexShaderOutput *v_out, const TriangleStageContext *contexts, int context_count) {

    RasterStage stage = {.fb = fb, .mesh = mesh, .v_out = v_out, .contexts = contexts, .context_count = context_count};
    atomic_init(&stage.next_tile, 0);
    thread_pool_run(pool, raster_stage_task, &stage);
}

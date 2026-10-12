#include "pipeline.h"

#include <stdlib.h>
#include <string.h>
#include "clip.h"


// Triangles a thread processes each time it takes work from the shared counter
#define TRIANGLE_CHUNK_SIZE 1024

// Clip vertex slots every thread gets however small the mesh is
#define CLIP_MIN_CAPACITY 4096

#define DEGENERATE_DOUBLE_AREA 1e-6f

typedef struct {
    // On their own cache line, threads keep bumping them while the fields below are only read
    _Alignas(CACHE_LINE_SIZE) atomic_size_t next_triangle;
    atomic_int next_context;

    _Alignas(CACHE_LINE_SIZE) const FrameBuffer *fb;
    const Mesh *mesh;
    const VertexShaderOutput *v_out;
    TriangleStageContext *contexts;
} TriangleStage;

static bool triangle_stage_context_init(TriangleStageContext *tc, const FrameBuffer *fb, size_t clip_capacity) {

    *tc = (TriangleStageContext){.clip_capacity = clip_capacity};

    tc->clip_v_out = malloc(sizeof(VertexShaderOutput) * clip_capacity);
    tc->clip_index_buffer = malloc(sizeof(uint32_t) * 3 * clip_capacity);
    tc->clip_material_ids = malloc(sizeof(uint32_t) * clip_capacity);

    bool bins_ok = tile_bins_init(&tc->tile_bins, fb->width, fb->height);

    return bins_ok && tc->clip_v_out && tc->clip_index_buffer && tc->clip_material_ids;
}

static void triangle_stage_context_free(TriangleStageContext *tc) {

    tile_bins_free(&tc->tile_bins);
    free(tc->clip_material_ids);
    free(tc->clip_index_buffer);
    free(tc->clip_v_out);
}

TriangleStageContext *triangle_stage_contexts_create(const FrameBuffer *fb, const Mesh *mesh, int context_count) {

    // 50% of the shaded vertices covers the clipping seen in tests. Threads take triangles in small chunks,
    // so the clipped ones spread out evenly enough to split that space equally between them
    size_t clip_capacity = mesh->vertex_count / 2 / context_count;
    clip_capacity = clip_capacity > CLIP_MIN_CAPACITY ? clip_capacity : CLIP_MIN_CAPACITY;

    TriangleStageContext *contexts = alloc_cache_aligned(sizeof(TriangleStageContext) * context_count);
    if (!contexts) {
        return NULL;
    }

    // Zeroed so triangle_stage_context_free is safe on contexts that never got initialized
    memset(contexts, 0, sizeof(TriangleStageContext) * context_count);

    for (int i = 0; i < context_count; ++i) {
        if (!triangle_stage_context_init(&contexts[i], fb, clip_capacity)) {
            triangle_stage_contexts_free(contexts, context_count);
            return NULL;
        }
    }

    return contexts;
}

void triangle_stage_contexts_free(TriangleStageContext *contexts, int context_count) {

    for (int i = 0; contexts && i < context_count; ++i) {
        triangle_stage_context_free(&contexts[i]);
    }
    free(contexts);
}

static bool setup_triangle(vec2s a, vec2s b, vec2s c) {

    float double_area = edge_function(a, b, c);

    // Cull back faces (negative area) and degenerate triangles (zero or near zero arear)
    if (double_area < DEGENERATE_DOUBLE_AREA) {
        return false;
    }

    return true;
}

static void process_triangle(const TriangleStage *stage, TriangleStageContext *tc, uint32_t triangle_id) {

    const Mesh *mesh = stage->mesh;
    const VertexShaderOutput *v_out = stage->v_out;
    size_t mesh_triangle_count = mesh->index_count / 3;

    const VertexShaderOutput *v0, *v1, *v2;
    fetch_triangle(v_out, &mesh->index_buffer[3 * triangle_id], &v0, &v1, &v2);

    // All three vertices outside the same plane means the whole triangle is, so skip it.
    // TODO: Masks are recomputed for shared vertices
    uint8_t mask0 = frustum_outside_mask(v0->clip);
    uint8_t mask1 = frustum_outside_mask(v1->clip);
    uint8_t mask2 = frustum_outside_mask(v2->clip);

    uint8_t shared_outside = mask0 & mask1 & mask2;
    if (shared_outside) {
        return;
    }

    // Only planes crossed by at least one vertex need clipping
    uint8_t any_outside = mask0 | mask1 | mask2;

    // Every triangle the clipper fans out shares the source triangle's material
    uint32_t material_id = mesh->material_ids[triangle_id];

    // Fully inside, the vertices are already in screen space so use them in place
    if (!any_outside) {
        vec2s a = screen_xy(v0);
        vec2s b = screen_xy(v1);
        vec2s c = screen_xy(v2);

        if (setup_triangle(a, b, c)) {
            bin_triangle(&tc->tile_bins, a, b, c, triangle_id);
        }
        return;
    }

    // The clipper writes the whole polygon, kept vertices included, so it can need up to MAX_CLIPPED_VERTICES slots.
    // TODO: In our testing the clip buffers are big enough, triangles that don't fit are dropped for now and will be handled later
    if (tc->clip_capacity - tc->clip_vertex_count < MAX_CLIPPED_VERTICES) {
        return;
    }

    uint32_t polygon_start = (uint32_t)tc->clip_vertex_count;
    VertexShaderOutput *clipped = &tc->clip_v_out[polygon_start];

    int clipped_count = clip_triangle_to_frustum(v0, v1, v2, any_outside, clipped);
    tc->clip_vertex_count += clipped_count;

    // Clipping creates new vertices, so the divide has to happen after it
    for (int j = 0; j < clipped_count; ++j) {
        clipped[j].screen = clip_to_viewport(stage->fb, clipped[j].clip);
    }

    // Fan triangulate the clipped polygon
    for (int j = 1; j + 1 < clipped_count; ++j) {

        uint32_t c0_index = polygon_start;
        uint32_t c1_index = polygon_start + j;
        uint32_t c2_index = polygon_start + j + 1;

        vec2s a = screen_xy(&tc->clip_v_out[c0_index]);
        vec2s b = screen_xy(&tc->clip_v_out[c1_index]);
        vec2s c = screen_xy(&tc->clip_v_out[c2_index]);

        // The polygon is already in screen winding, store it reversed so fetch_triangle restores it
        if (setup_triangle(a, b, c)) {
            tc->clip_index_buffer[3 * tc->clip_triangle_count] = c0_index;
            tc->clip_index_buffer[3 * tc->clip_triangle_count + 1] = c2_index;
            tc->clip_index_buffer[3 * tc->clip_triangle_count + 2] = c1_index;
            tc->clip_material_ids[tc->clip_triangle_count] = material_id;

            uint32_t clip_triangle_id = (uint32_t)(mesh_triangle_count + tc->clip_triangle_count);
            tc->clip_triangle_count++;

            bin_triangle(&tc->tile_bins, a, b, c, clip_triangle_id);
        }
    }
}

static void triangle_stage_task(void *ctx) {

    TriangleStage *stage = ctx;
    size_t triangle_count = stage->mesh->index_count / 3;

    // Every thread runs the task once per dispatch, so this gives each one its own context
    int context_index = atomic_fetch_add_explicit(&stage->next_context, 1, memory_order_relaxed);
    TriangleStageContext *tc = &stage->contexts[context_index];

    size_t start, end;
    while (claim_range(&stage->next_triangle, TRIANGLE_CHUNK_SIZE, triangle_count, &start, &end)) {
        for (size_t t = start; t < end; ++t) {
            process_triangle(stage, tc, (uint32_t)t);
        }
    }
}

void triangle_stage_run(ThreadPool *pool, const FrameBuffer *fb, const Mesh *mesh, const VertexShaderOutput *v_out, TriangleStageContext *contexts) {

    TriangleStage stage = {.fb = fb, .mesh = mesh, .v_out = v_out, .contexts = contexts};
    atomic_init(&stage.next_triangle, 0);
    atomic_init(&stage.next_context, 0);
    thread_pool_run(pool, triangle_stage_task, &stage);
}

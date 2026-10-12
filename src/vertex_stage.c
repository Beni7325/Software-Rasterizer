#include "pipeline.h"


// Vertices a thread shades each time it takes work from the shared counter
#define VERTEX_CHUNK_SIZE 1024

typedef struct {
    // On its own cache line, to avoid false sharing when threads update the atomic counter
    _Alignas(CACHE_LINE_SIZE) atomic_size_t next_vertex;

    _Alignas(CACHE_LINE_SIZE) const FrameBuffer *fb;
    const Mesh *mesh;
    const Uniforms *uniforms;
    VertexShaderOutput *v_out;
} VertexStage;

static void vertex_stage_task(void *ctx) {

    VertexStage *stage = ctx;
    size_t vertex_count = stage->mesh->vertex_count;

    size_t start, end;
    while (claim_range(&stage->next_vertex, VERTEX_CHUNK_SIZE, vertex_count, &start, &end)) {
        for (size_t i = start; i < end; ++i) {
            vertex_shader(&stage->mesh->vertex_buffer[i], stage->uniforms, &stage->v_out[i]);
            stage->v_out[i].screen = clip_to_viewport(stage->fb, stage->v_out[i].clip);
        }
    }
}

void vertex_stage_run(ThreadPool *pool, const FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms, VertexShaderOutput *v_out) {

    VertexStage stage = {.fb = fb, .mesh = mesh, .uniforms = uniforms, .v_out = v_out};
    atomic_init(&stage.next_vertex, 0);
    thread_pool_run(pool, vertex_stage_task, &stage);
}

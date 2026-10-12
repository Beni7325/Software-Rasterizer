#include "renderer.h"

#include <stdlib.h>
#include "pipeline.h"


void draw_mesh(FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms, ThreadPool *pool) {

    // The calling thread runs the tasks too, each one gets its own triangle stage context
    int thread_count = pool->worker_count + 1;

    // Cache line aligned so the chunks the vertex stage threads write never share a line
    // TODO: Use some kind of arena to avoid calling malloc each time
    VertexShaderOutput *v_out = alloc_cache_aligned(sizeof(VertexShaderOutput) * mesh->vertex_count);
    TriangleStageContext *contexts = triangle_stage_contexts_create(fb, mesh, thread_count);

    if (v_out && contexts) {
        vertex_stage_run(pool, fb, mesh, uniforms, v_out);
        triangle_stage_run(pool, fb, mesh, v_out, contexts);
        raster_stage_run(pool, fb, mesh, v_out, contexts, thread_count);
    }

    triangle_stage_contexts_free(contexts, thread_count);
    free(v_out);
}

void draw_model(FrameBuffer *fb, const Model *model, const Camera *camera, ThreadPool *pool) {

    mat4s model_mat = model_matrix(model);
    Uniforms uniforms = {.mvp = glms_mat4_mul(camera->view_projection, model_mat)};

    draw_mesh(fb, model->mesh, &uniforms, pool);
}

#ifndef PIPELINE_H
#define PIPELINE_H

// Internal to the renderer: the stages draw_mesh runs and what they share

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <cglm/struct.h>
#include "framebuffer.h"
#include "mesh.h"
#include "shader.h"
#include "thread_pool.h"
#include "tile.h"
#include "util.h"


typedef struct {
    // Aligned so every context starts on its own cache line, otherwise threads bumping the clip counts of neighbouring
    // contexts would cause false sharing
    _Alignas(CACHE_LINE_SIZE) TileBins tile_bins;

    // Clipping creates new vertices and triangles, they go in this context's own buffers: vertices in clip_v_out,
    // triangles as 3 indices into clip_v_out in clip_index_buffer and 1 material id each in clip_material_ids
    VertexShaderOutput *clip_v_out;
    uint32_t *clip_index_buffer;
    uint32_t *clip_material_ids;
    size_t clip_capacity;
    size_t clip_vertex_count;
    size_t clip_triangle_count;
} TriangleStageContext;

vec4s clip_to_viewport(const FrameBuffer *fb, vec4s clip);
vec2s screen_xy(const VertexShaderOutput *v);
void fetch_triangle(const VertexShaderOutput *vertices, const uint32_t *indices, const VertexShaderOutput **v0, const VertexShaderOutput **v1, const VertexShaderOutput **v2);
bool claim_range(atomic_size_t *next, size_t chunk_size, size_t count, size_t *start, size_t *end);

TriangleStageContext *triangle_stage_contexts_create(const FrameBuffer *fb, const Mesh *mesh, int context_count);
void triangle_stage_contexts_free(TriangleStageContext *contexts, int context_count);

// Shades every mesh vertex into v_out and transforms it to screen space
void vertex_stage_run(ThreadPool *pool, const FrameBuffer *fb, const Mesh *mesh, const Uniforms *uniforms, VertexShaderOutput *v_out);

// Culls, clips and bins every mesh triangle into one context per thread
void triangle_stage_run(ThreadPool *pool, const FrameBuffer *fb, const Mesh *mesh, const VertexShaderOutput *v_out, TriangleStageContext *contexts);

// Rasterizes the binned triangles of every context, one tile per thread at a time
void raster_stage_run(ThreadPool *pool, FrameBuffer *fb, const Mesh *mesh, const VertexShaderOutput *v_out, const TriangleStageContext *contexts, int context_count);

#endif /* PIPELINE_H */

#include "tile.h"

#include <math.h>
#include <stdlib.h>
#include "util.h"


bool tile_bins_init(TileBins *tb, int width, int height) {

    // The size is a multiple of TILE_SIZE, so the tiles cover it exactly
    tb->tiles = (ivec2s){.x = width / TILE_SIZE, .y = height / TILE_SIZE};
    tb->bins = calloc(tb->tiles.x * tb->tiles.y, sizeof(BinChunk *));

    return tb->bins != NULL;
}

void tile_bins_free(TileBins *tb) {

    if (!tb->bins) {
        return;
    }

    int tile_count = tb->tiles.x * tb->tiles.y;

    for (int i = 0; i < tile_count; ++i) {

        BinChunk *chunk = tb->bins[i];

        while (chunk) {
            BinChunk *next = chunk->next;
            free(chunk);
            chunk = next;
        }
    }

    free(tb->bins);
    tb->bins = NULL;
}

static void bin_push(BinChunk **bin, uint32_t triangle_id) {

    BinChunk *head = *bin;

    if (!head || head->count == BIN_CHUNK_CAPACITY) {
        // TODO: Use some kind of arena to avoid calling malloc each time
        BinChunk *chunk = malloc(sizeof(BinChunk));
        if (!chunk) {
            return;
        }

        chunk->next = head;
        chunk->count = 0;
        *bin = head = chunk;
    }

    head->triangle_ids[head->count++] = triangle_id;
}

void bin_triangle(TileBins *tb, vec2s a, vec2s b, vec2s c, uint32_t triangle_id) {

    vec2s bb_min = min3_vec2(a, b, c);
    vec2s bb_max = max3_vec2(a, b, c);

    ivec2s tile_min = {.x = (int)floorf(bb_min.x / TILE_SIZE), .y = (int)floorf(bb_min.y / TILE_SIZE)};
    ivec2s tile_max = {.x = (int)floorf(bb_max.x / TILE_SIZE), .y = (int)floorf(bb_max.y / TILE_SIZE)};

    // Clamp to the tiles on screen
    ivec2s first_tile = {.x = 0, .y = 0};
    ivec2s last_tile = {.x = tb->tiles.x - 1, .y = tb->tiles.y - 1};

    tile_min = glms_ivec2_maxv(tile_min, first_tile);
    tile_max = glms_ivec2_minv(tile_max, last_tile);

    for (int ty = tile_min.y; ty <= tile_max.y; ++ty) {
        for (int tx = tile_min.x; tx <= tile_max.x; ++tx) {
            bin_push(&tb->bins[ty * tb->tiles.x + tx], triangle_id);
        }
    }
}

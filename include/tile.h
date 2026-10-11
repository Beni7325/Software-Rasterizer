#ifndef TILE_H
#define TILE_H

#include <stdbool.h>
#include <stdint.h>
#include <cglm/struct.h>


#define TILE_SIZE 16

// Sized so a chunk is 64 bytes
#define BIN_CHUNK_CAPACITY 13

typedef struct BinChunk {
    struct BinChunk *next;
    uint32_t count;
    uint32_t triangle_ids[BIN_CHUNK_CAPACITY];
} BinChunk;

typedef struct {
    ivec2s tiles;  // Tile count in x and y
    BinChunk **bins;
} TileBins;

bool tile_bins_init(TileBins *tb, int width, int height);
void tile_bins_free(TileBins *tb);
void bin_triangle(TileBins *tb, vec2s a, vec2s b, vec2s c, uint32_t triangle_id);

#endif /* TILE_H */

#ifndef UTIL_H
#define UTIL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <cglm/struct.h>

#define CACHE_LINE_SIZE 64

// a * weights.x + b * weights.y + c * weights.z, e.g. barycentric interpolation of a per vertex value
static inline vec3s weighted_sum_vec3(vec3s a, vec3s b, vec3s c, vec3s weights) {

    vec3s weighted_a = glms_vec3_scale(a, weights.x);
    vec3s weighted_b = glms_vec3_scale(b, weights.y);
    vec3s weighted_c = glms_vec3_scale(c, weights.z);

    vec3s sum_ab = glms_vec3_add(weighted_a, weighted_b);
    return glms_vec3_add(sum_ab, weighted_c);
}

static inline vec2s weighted_sum_vec2(vec2s a, vec2s b, vec2s c, vec3s weights) {

    vec2s weighted_a = glms_vec2_scale(a, weights.x);
    vec2s weighted_b = glms_vec2_scale(b, weights.y);
    vec2s weighted_c = glms_vec2_scale(c, weights.z);

    vec2s sum_ab = glms_vec2_add(weighted_a, weighted_b);
    return glms_vec2_add(sum_ab, weighted_c);
}

// Twice the signed area of triangle (a, b, p): positive when p is to the left of a -> b
static inline float edge_function(vec2s a, vec2s b, vec2s p) {

    vec2s ab = glms_vec2_sub(b, a);
    vec2s ap = glms_vec2_sub(p, a);

    return glms_vec2_cross(ab, ap);
}

static inline vec2s min3_vec2(vec2s a, vec2s b, vec2s c) {
    vec2s min_bc = glms_vec2_minv(b, c);
    return glms_vec2_minv(a, min_bc);
}

static inline vec2s max3_vec2(vec2s a, vec2s b, vec2s c) {
    vec2s max_bc = glms_vec2_maxv(b, c);
    return glms_vec2_maxv(a, max_bc);
}

// Grows the box [*bb_min, *bb_max] to contain p
static inline void aabb_extend(vec3s *bb_min, vec3s *bb_max, vec3s p) {
    *bb_min = glms_vec3_minv(*bb_min, p);
    *bb_max = glms_vec3_maxv(*bb_max, p);
}

// Fractional part, always in [0, 1) also for negative x
static inline float fractf(float x) {
    return x - floorf(x);
}

static inline vec2s fract_vec2(vec2s v) {
    return (vec2s){.x = fractf(v.x), .y = fractf(v.y)};
}

// Element index of an array of tightly packed vectors
static inline vec3s vec3_from_array(const float *array, size_t index) {
    const float *v = &array[3 * index];
    return (vec3s){.x = v[0], .y = v[1], .z = v[2]};
}

static inline vec2s vec2_from_array(const float *array, size_t index) {
    const float *v = &array[2 * index];
    return (vec2s){.x = v[0], .y = v[1]};
}

// Smallest power of two >= n, 1 for n == 0
static inline size_t next_pow2(size_t n) {

    size_t pow2 = 1;
    while (pow2 < n) {
        pow2 *= 2;
    }

    return pow2;
}

// Packs a color with channels in [0, 255] into RGBA8888 with full alpha
static inline uint32_t rgba(vec3s color) {
    return ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) | ((uint32_t)color.b << 8) | 255;
}

// aligned_alloc on a cache line boundary, rounding size up to a multiple of the alignment as aligned_alloc requires
void *alloc_cache_aligned(size_t size);

// Reads a whole file into a malloc'd buffer (not null terminated). Returns NULL on failure or an empty file
char *read_file(const char *path, size_t *len);

// Resolves relative against the directory of base_file, e.g. ("models/a.obj", "tex/b.png") -> "models/tex/b.png". Returns a malloc'd string
char *path_join_dir(const char *base_file, const char *relative);

#endif /* UTIL_H */

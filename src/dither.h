/*
 * SLEER - palette dithering engine (portable C11, no dependencies)
 *
 * The engine maps RGB pixels onto a fixed palette. Error diffusion kernels,
 * ordered (Bayer), random and plain nearest-colour modes are supported, so the
 * same code covers 1-bit output and full retro palettes.
 *
 * Before dithering, prepare a SleerQuantizer for the palette once (it holds a
 * nearest-colour lookup table); re-dithering with new settings is then cheap.
 */
#ifndef SLEER_DITHER_H
#define SLEER_DITHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SleerColor;

#define SLEER_MAX_PALETTE_COLORS 512
#define SLEER_MIN_PALETTE_COLORS 2

typedef enum {
    SLEER_TYPE_DIFFUSION = 0, /* error diffusion across neighbours */
    SLEER_TYPE_ORDERED,       /* ordered / Bayer threshold matrix */
    SLEER_TYPE_RANDOM,        /* white-noise threshold */
    SLEER_TYPE_NONE,          /* nearest colour only */
    SLEER_TYPE_COUNT
} SleerDitherType;

typedef enum {
    SLEER_KERNEL_FLOYD_STEINBERG = 0,
    SLEER_KERNEL_FALSE_FLOYD_STEINBERG,
    SLEER_KERNEL_JARVIS,
    SLEER_KERNEL_STUCKI,
    SLEER_KERNEL_BURKES,
    SLEER_KERNEL_SIERRA3,
    SLEER_KERNEL_SIERRA2,
    SLEER_KERNEL_SIERRA_LITE,
    SLEER_KERNEL_ATKINSON,
    SLEER_KERNEL_COUNT
} SleerKernel;

typedef struct {
    SleerDitherType type;
    SleerKernel kernel;
    int matrix_size; /* ordered only: 2, 4, 8 or 16 */
    float bias;      /* brightness offset, -1.0 .. 1.0 */
    int serpentine;  /* diffusion only: alternate the scan direction */
    uint32_t seed;   /* random only */
} SleerParams;

#define SLEER_LUT_BITS 5
#define SLEER_LUT_LEVELS (1 << SLEER_LUT_BITS)
#define SLEER_LUT_SIZE (SLEER_LUT_LEVELS * SLEER_LUT_LEVELS * SLEER_LUT_LEVELS)

typedef struct {
    const SleerColor *colors;
    int count;
    /* Mean distance between neighbouring palette colours; ordered and random
     * dithering use it as the threshold amplitude. */
    float amplitude;
    uint16_t lut[SLEER_LUT_SIZE];
} SleerQuantizer;

void sleer_params_defaults(SleerParams *params);

/* Builds the nearest-colour table. Returns 0, or -1 for an invalid palette. */
int sleer_quantizer_init(SleerQuantizer *quantizer, const SleerColor *colors, int count);

/*
 * Dithered RGBA output. `out_rgba` may be the same buffer as `rgba`.
 * Returns 0 on success, -1 on invalid arguments.
 */
int sleer_dither_rgba(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                      int height, const SleerParams *params, uint8_t *out_rgba);

/* Bayer threshold matrix, values 0 .. size*size-1. `out` needs size*size bytes. */
void sleer_bayer_matrix(int size, uint8_t *out);

const char *sleer_kernel_name(SleerKernel kernel);
const char *sleer_kernel_id(SleerKernel kernel);
int sleer_kernel_from_string(const char *name, SleerKernel *out);

const char *sleer_type_name(SleerDitherType type);
const char *sleer_type_id(SleerDitherType type);
int sleer_type_from_string(const char *name, SleerDitherType *out);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_DITHER_H */

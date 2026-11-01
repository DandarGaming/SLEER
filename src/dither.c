#include "dither.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------- metadata */

static const char *const k_kernel_names[SLEER_KERNEL_COUNT] = {
    "Floyd-Steinberg",
    "False Floyd-Steinberg",
    "Jarvis-Judice-Ninke",
    "Stucki",
    "Burkes",
    "Sierra 3",
    "Sierra 2",
    "Sierra Lite",
    "Atkinson",
};

static const char *const k_kernel_ids[SLEER_KERNEL_COUNT] = {
    "floyd-steinberg",
    "false-floyd-steinberg",
    "jarvis",
    "stucki",
    "burkes",
    "sierra3",
    "sierra2",
    "sierra-lite",
    "atkinson",
};

static const char *const k_type_names[SLEER_TYPE_COUNT] = {
    "Error diffusion",
    "Ordered",
    "Random",
    "Colour reduction",
};

static const char *const k_type_ids[SLEER_TYPE_COUNT] = {
    "diffusion",
    "ordered",
    "random",
    "none",
};

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float clamp_float(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

const char *sleer_kernel_name(SleerKernel kernel)
{
    if ((int)kernel < 0 || kernel >= SLEER_KERNEL_COUNT) {
        return "Unknown";
    }
    return k_kernel_names[kernel];
}

const char *sleer_kernel_id(SleerKernel kernel)
{
    if ((int)kernel < 0 || kernel >= SLEER_KERNEL_COUNT) {
        return "unknown";
    }
    return k_kernel_ids[kernel];
}

int sleer_kernel_from_string(const char *name, SleerKernel *out)
{
    int i;

    if (!name || !out) {
        return -1;
    }
    for (i = 0; i < SLEER_KERNEL_COUNT; i++) {
        if (strcmp(name, k_kernel_ids[i]) == 0 || strcmp(name, k_kernel_names[i]) == 0) {
            *out = (SleerKernel)i;
            return 0;
        }
    }
    return -1;
}

const char *sleer_type_name(SleerDitherType type)
{
    if ((int)type < 0 || type >= SLEER_TYPE_COUNT) {
        return "Unknown";
    }
    return k_type_names[type];
}

const char *sleer_type_id(SleerDitherType type)
{
    if ((int)type < 0 || type >= SLEER_TYPE_COUNT) {
        return "unknown";
    }
    return k_type_ids[type];
}

int sleer_type_from_string(const char *name, SleerDitherType *out)
{
    int i;

    if (!name || !out) {
        return -1;
    }
    for (i = 0; i < SLEER_TYPE_COUNT; i++) {
        if (strcmp(name, k_type_ids[i]) == 0 || strcmp(name, k_type_names[i]) == 0) {
            *out = (SleerDitherType)i;
            return 0;
        }
    }
    return -1;
}

void sleer_params_defaults(SleerParams *params)
{
    if (!params) {
        return;
    }
    params->type = SLEER_TYPE_DIFFUSION;
    params->kernel = SLEER_KERNEL_FLOYD_STEINBERG;
    params->matrix_size = 8;
    params->bias = 0.0f;
    params->serpentine = 1;
    params->seed = 0x5eed1234u;
}

/* ------------------------------------------------------------------ kernels */

#define SLEER_MAX_TAPS 12

typedef struct {
    int8_t dx;
    uint8_t dy;
    uint8_t numerator;
    uint8_t denominator;
} KernelTap;

#define TAP(dx_, dy_, num_, den_) {dx_, dy_, num_, den_}

/*
 * Classic diffusion kernels. Weights are normalised per tap so the table needs
 * no divisor at runtime.
 */
static const KernelTap k_fs_taps[] = {
    TAP(1, 0, 7, 16),
    TAP(-1, 1, 3, 16),
    TAP(0, 1, 5, 16),
    TAP(1, 1, 1, 16),
};

static const KernelTap k_false_fs_taps[] = {
    TAP(1, 0, 3, 8),
    TAP(0, 1, 3, 8),
    TAP(1, 1, 2, 8),
};

static const KernelTap k_jarvis_taps[] = {
    TAP(1, 0, 7, 48),
    TAP(2, 0, 5, 48),
    TAP(-2, 1, 3, 48),
    TAP(-1, 1, 5, 48),
    TAP(0, 1, 7, 48),
    TAP(1, 1, 5, 48),
    TAP(2, 1, 3, 48),
    TAP(-2, 2, 1, 48),
    TAP(-1, 2, 3, 48),
    TAP(0, 2, 5, 48),
    TAP(1, 2, 3, 48),
    TAP(2, 2, 1, 48),
};

static const KernelTap k_stucki_taps[] = {
    TAP(1, 0, 8, 42),
    TAP(2, 0, 4, 42),
    TAP(-2, 1, 2, 42),
    TAP(-1, 1, 4, 42),
    TAP(0, 1, 8, 42),
    TAP(1, 1, 4, 42),
    TAP(2, 1, 2, 42),
    TAP(-2, 2, 1, 42),
    TAP(-1, 2, 2, 42),
    TAP(0, 2, 4, 42),
    TAP(1, 2, 2, 42),
    TAP(2, 2, 1, 42),
};

static const KernelTap k_burkes_taps[] = {
    TAP(1, 0, 8, 32),
    TAP(2, 0, 4, 32),
    TAP(-2, 1, 2, 32),
    TAP(-1, 1, 4, 32),
    TAP(0, 1, 8, 32),
    TAP(1, 1, 4, 32),
    TAP(2, 1, 2, 32),
};

static const KernelTap k_sierra3_taps[] = {
    TAP(1, 0, 5, 32),
    TAP(2, 0, 3, 32),
    TAP(-2, 1, 2, 32),
    TAP(-1, 1, 4, 32),
    TAP(0, 1, 5, 32),
    TAP(1, 1, 4, 32),
    TAP(2, 1, 2, 32),
    TAP(-1, 2, 2, 32),
    TAP(0, 2, 3, 32),
    TAP(1, 2, 2, 32),
};

static const KernelTap k_sierra2_taps[] = {
    TAP(1, 0, 4, 16),
    TAP(2, 0, 3, 16),
    TAP(-2, 1, 1, 16),
    TAP(-1, 1, 2, 16),
    TAP(0, 1, 3, 16),
    TAP(1, 1, 2, 16),
    TAP(2, 1, 1, 16),
};

static const KernelTap k_sierra_lite_taps[] = {
    TAP(1, 0, 2, 4),
    TAP(-1, 1, 1, 4),
    TAP(0, 1, 1, 4),
};

static const KernelTap k_atkinson_taps[] = {
    TAP(1, 0, 1, 8),
    TAP(2, 0, 1, 8),
    TAP(-1, 1, 1, 8),
    TAP(0, 1, 1, 8),
    TAP(1, 1, 1, 8),
    TAP(0, 2, 1, 8),
};

typedef struct {
    const KernelTap *taps;
    int count;
} KernelDef;

static const KernelDef k_kernels[SLEER_KERNEL_COUNT] = {
    {k_fs_taps, (int)(sizeof(k_fs_taps) / sizeof(k_fs_taps[0]))},
    {k_false_fs_taps, (int)(sizeof(k_false_fs_taps) / sizeof(k_false_fs_taps[0]))},
    {k_jarvis_taps, (int)(sizeof(k_jarvis_taps) / sizeof(k_jarvis_taps[0]))},
    {k_stucki_taps, (int)(sizeof(k_stucki_taps) / sizeof(k_stucki_taps[0]))},
    {k_burkes_taps, (int)(sizeof(k_burkes_taps) / sizeof(k_burkes_taps[0]))},
    {k_sierra3_taps, (int)(sizeof(k_sierra3_taps) / sizeof(k_sierra3_taps[0]))},
    {k_sierra2_taps, (int)(sizeof(k_sierra2_taps) / sizeof(k_sierra2_taps[0]))},
    {k_sierra_lite_taps, (int)(sizeof(k_sierra_lite_taps) / sizeof(k_sierra_lite_taps[0]))},
    {k_atkinson_taps, (int)(sizeof(k_atkinson_taps) / sizeof(k_atkinson_taps[0]))},
};

/* ---------------------------------------------------------------- quantizer */

void sleer_bayer_matrix(int size, uint8_t *out)
{
    int source[256];
    int target[256];
    int current = 1;
    int x, y;

    if (!out) {
        return;
    }
    if (size != 2 && size != 4 && size != 8 && size != 16) {
        size = 8;
    }

    source[0] = 0;
    while (current < size) {
        const int next = current * 2;
        for (y = 0; y < current; y++) {
            for (x = 0; x < current; x++) {
                const int base = source[y * current + x] * 4;
                target[y * next + x] = base;
                target[y * next + x + current] = base + 2;
                target[(y + current) * next + x] = base + 3;
                target[(y + current) * next + x + current] = base + 1;
            }
        }
        memcpy(source, target, sizeof(int) * (size_t)(next * next));
        current = next;
    }

    for (y = 0; y < size * size; y++) {
        out[y] = (uint8_t)source[y];
    }
}

static int color_distance2(const SleerColor *a, const SleerColor *b)
{
    const int dr = (int)a->r - (int)b->r;
    const int dg = (int)a->g - (int)b->g;
    const int db = (int)a->b - (int)b->b;
    return dr * dr + dg * dg + db * db;
}

int sleer_quantizer_init(SleerQuantizer *quantizer, const SleerColor *colors, int count)
{
    int i, j;
    double total = 0.0;
    int x, y, z;

    if (!quantizer || !colors || count < SLEER_MIN_PALETTE_COLORS ||
        count > SLEER_MAX_PALETTE_COLORS) {
        return -1;
    }

    quantizer->colors = colors;
    quantizer->count = count;

    /* Threshold amplitude for ordered/random dithering: how far apart the
     * palette colours sit. A black/white palette ends up at full range, which
     * is what classic Bayer dithering expects. */
    for (i = 0; i < count; i++) {
        int best = 1 << 30;

        for (j = 0; j < count; j++) {
            int distance;

            if (i == j) {
                continue;
            }
            distance = color_distance2(&colors[i], &colors[j]);
            if (distance < best) {
                best = distance;
            }
        }
        total += sqrt((double)best);
    }
    quantizer->amplitude = (float)clamp_float((float)(total / (double)count), 6.0f, 255.0f);

    for (z = 0; z < SLEER_LUT_LEVELS; z++) {
        for (y = 0; y < SLEER_LUT_LEVELS; y++) {
            for (x = 0; x < SLEER_LUT_LEVELS; x++) {
                const SleerColor probe = {(uint8_t)(x * 8 + 4), (uint8_t)(y * 8 + 4),
                                          (uint8_t)(z * 8 + 4)};
                int best = 1 << 30;
                int best_index = 0;

                for (i = 0; i < count; i++) {
                    const int distance = color_distance2(&probe, &colors[i]);
                    if (distance < best) {
                        best = distance;
                        best_index = i;
                    }
                }
                quantizer->lut[(x << 10) | (y << 5) | z] = (uint16_t)best_index;
            }
        }
    }

    return 0;
}

/* 32 buckets of 8 values each, so the index shift is 8 - LUT_BITS. */
#define LUT_SHIFT (8 - SLEER_LUT_BITS)

static int lut_index(float r, float g, float b)
{
    const int ri = clamp_int((int)(r + 0.5f), 0, 255) >> LUT_SHIFT;
    const int gi = clamp_int((int)(g + 0.5f), 0, 255) >> LUT_SHIFT;
    const int bi = clamp_int((int)(b + 0.5f), 0, 255) >> LUT_SHIFT;

    return (ri << (SLEER_LUT_BITS * 2)) | (gi << SLEER_LUT_BITS) | bi;
}

static void write_out(uint8_t *out, size_t pixel, const SleerColor *color)
{
    uint8_t *target = out + pixel * 4;

    target[0] = color->r;
    target[1] = color->g;
    target[2] = color->b;
    target[3] = 255;
}

/* --------------------------------------------------------------- algorithms */

#define ROW_PAD 2

static int dither_diffusion(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                            int height, const SleerParams *params, uint8_t *out)
{
    const KernelDef *definition =
        &k_kernels[(int)params->kernel >= 0 && params->kernel < SLEER_KERNEL_COUNT
                       ? params->kernel
                       : SLEER_KERNEL_FLOYD_STEINBERG];
    const float bias = clamp_float(params->bias, -1.0f, 1.0f) * 255.0f;
    const int row_length = (width + ROW_PAD * 2) * 3;
    float weights[SLEER_MAX_TAPS];
    float *rows;
    int t;
    int x, y;

    for (t = 0; t < definition->count; t++) {
        weights[t] = (float)definition->taps[t].numerator /
                     (float)definition->taps[t].denominator;
    }

    rows = (float *)calloc((size_t)row_length * 3, sizeof(float));
    if (!rows) {
        return -1;
    }

    for (y = 0; y < height; y++) {
        const int direction = (params->serpentine && (y & 1)) ? -1 : 1;
        float *current = rows + (size_t)(y % 3) * (size_t)row_length;

        /* Row y + 2 only holds errors that were already consumed. */
        memset(rows + (size_t)((y + 2) % 3) * (size_t)row_length, 0,
               sizeof(float) * (size_t)row_length);

        for (x = 0; x < width; x++) {
            const int px = direction > 0 ? x : width - 1 - x;
            const size_t source = ((size_t)y * (size_t)width + (size_t)px) * 4;
            float *accumulated = current + (size_t)(px + ROW_PAD) * 3;
            float r = (float)rgba[source + 0] + accumulated[0] + bias;
            float g = (float)rgba[source + 1] + accumulated[1] + bias;
            float b = (float)rgba[source + 2] + accumulated[2] + bias;
            const SleerColor *color;
            float er, eg, eb;

            r = clamp_float(r, 0.0f, 255.0f);
            g = clamp_float(g, 0.0f, 255.0f);
            b = clamp_float(b, 0.0f, 255.0f);

            color = &quantizer->colors[quantizer->lut[lut_index(r, g, b)]];
            write_out(out, (size_t)y * (size_t)width + (size_t)px, color);

            er = r - (float)color->r;
            eg = g - (float)color->g;
            eb = b - (float)color->b;

            for (t = 0; t < definition->count; t++) {
                const KernelTap *tap = &definition->taps[t];
                const int target_x = px + tap->dx * direction;
                float *target;

                if (target_x < 0 || target_x >= width) {
                    continue;
                }
                target = rows + (size_t)((y + tap->dy) % 3) * (size_t)row_length +
                         (size_t)(target_x + ROW_PAD) * 3;
                target[0] += er * weights[t];
                target[1] += eg * weights[t];
                target[2] += eb * weights[t];
            }
        }
    }

    free(rows);
    return 0;
}

static int dither_ordered(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                          int height, const SleerParams *params, uint8_t *out)
{
    const float bias = clamp_float(params->bias, -1.0f, 1.0f) * 255.0f;
    const float amplitude = quantizer->amplitude;
    uint8_t matrix[256];
    int size = params->matrix_size;
    float denominator;
    int x, y;

    if (size != 2 && size != 4 && size != 8 && size != 16) {
        size = 8;
    }
    sleer_bayer_matrix(size, matrix);
    denominator = (float)(size * size);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const size_t index = (size_t)y * (size_t)width + (size_t)x;
            const size_t source = index * 4;
            const float threshold =
                ((float)matrix[(y & (size - 1)) * size + (x & (size - 1))] + 0.5f) / denominator;
            const float offset = (threshold - 0.5f) * amplitude + bias;
            const float r = clamp_float((float)rgba[source + 0] + offset, 0.0f, 255.0f);
            const float g = clamp_float((float)rgba[source + 1] + offset, 0.0f, 255.0f);
            const float b = clamp_float((float)rgba[source + 2] + offset, 0.0f, 255.0f);

            write_out(out, index,
                      &quantizer->colors[quantizer->lut[lut_index(r, g, b)]]);
        }
    }

    return 0;
}

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int dither_random(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                         int height, const SleerParams *params, uint8_t *out)
{
    const float bias = clamp_float(params->bias, -1.0f, 1.0f) * 255.0f;
    const float amplitude = quantizer->amplitude;
    uint32_t state = params->seed ? params->seed : 0x9e3779b9u;
    size_t i;
    const size_t pixels = (size_t)width * (size_t)height;

    for (i = 0; i < pixels; i++) {
        const size_t source = i * 4;
        const float noise = ((float)(xorshift32(&state) >> 8) / 16777216.0f) - 0.5f;
        const float offset = noise * amplitude + bias;
        const float r = clamp_float((float)rgba[source + 0] + offset, 0.0f, 255.0f);
        const float g = clamp_float((float)rgba[source + 1] + offset, 0.0f, 255.0f);
        const float b = clamp_float((float)rgba[source + 2] + offset, 0.0f, 255.0f);

        write_out(out, i, &quantizer->colors[quantizer->lut[lut_index(r, g, b)]]);
    }

    return 0;
}

static int dither_none(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                       int height, const SleerParams *params, uint8_t *out)
{
    const float bias = clamp_float(params->bias, -1.0f, 1.0f) * 255.0f;
    size_t i;
    const size_t pixels = (size_t)width * (size_t)height;

    for (i = 0; i < pixels; i++) {
        const size_t source = i * 4;
        const float r = clamp_float((float)rgba[source + 0] + bias, 0.0f, 255.0f);
        const float g = clamp_float((float)rgba[source + 1] + bias, 0.0f, 255.0f);
        const float b = clamp_float((float)rgba[source + 2] + bias, 0.0f, 255.0f);

        write_out(out, i, &quantizer->colors[quantizer->lut[lut_index(r, g, b)]]);
    }

    return 0;
}

int sleer_dither_rgba(const SleerQuantizer *quantizer, const uint8_t *rgba, int width,
                      int height, const SleerParams *params, uint8_t *out_rgba)
{
    if (!quantizer || !quantizer->colors || quantizer->count < SLEER_MIN_PALETTE_COLORS ||
        !rgba || !params || !out_rgba || width <= 0 || height <= 0) {
        return -1;
    }

    switch (params->type) {
    case SLEER_TYPE_DIFFUSION:
        return dither_diffusion(quantizer, rgba, width, height, params, out_rgba);
    case SLEER_TYPE_ORDERED:
        return dither_ordered(quantizer, rgba, width, height, params, out_rgba);
    case SLEER_TYPE_RANDOM:
        return dither_random(quantizer, rgba, width, height, params, out_rgba);
    case SLEER_TYPE_NONE:
        return dither_none(quantizer, rgba, width, height, params, out_rgba);
    default:
        return -1;
    }
}

#include "palette.h"

#include <stdlib.h>
#include <string.h>

static SleerColor rgb(uint8_t r, uint8_t g, uint8_t b)
{
    SleerColor color;

    color.r = r;
    color.g = g;
    color.b = b;
    return color;
}

/* --------------------------------------------------------------- palettes */

void sleer_palette_gray(int levels, SleerPaletteData *out)
{
    int i;

    if (!out) {
        return;
    }
    if (levels < 2) {
        levels = 2;
    }
    if (levels > SLEER_MAX_PALETTE_COLORS) {
        levels = SLEER_MAX_PALETTE_COLORS;
    }

    out->count = levels;
    for (i = 0; i < levels; i++) {
        const uint8_t value = (uint8_t)((i * 255 + (levels - 1) / 2) / (levels - 1));
        out->colors[i] = rgb(value, value, value);
    }
}

void sleer_palette_rgb_bits(int bits, SleerPaletteData *out)
{
    const int levels = 1 << bits;
    int r, g, b;
    int index = 0;

    if (!out) {
        return;
    }
    if (bits < 1) {
        bits = 1;
    }
    if (bits > 3) {
        bits = 3; /* 512 colours is our ceiling */
    }

    for (r = 0; r < levels; r++) {
        for (g = 0; g < levels; g++) {
            for (b = 0; b < levels; b++) {
                const uint8_t rv = (uint8_t)(r * 255 / (levels - 1));
                const uint8_t gv = (uint8_t)(g * 255 / (levels - 1));
                const uint8_t bv = (uint8_t)(b * 255 / (levels - 1));
                out->colors[index++] = rgb(rv, gv, bv);
            }
        }
    }
    out->count = index;
}

static void palette_from_list(SleerPaletteData *out, const SleerColor *colors, int count)
{
    int i;

    out->count = count > SLEER_MAX_PALETTE_COLORS ? SLEER_MAX_PALETTE_COLORS : count;
    for (i = 0; i < out->count; i++) {
        out->colors[i] = colors[i];
    }
}

static void build_black_white(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},
        {255, 255, 255},
    };

    palette_from_list(out, colors, 2);
}

static void build_gray_2bit(SleerPaletteData *out)
{
    sleer_palette_gray(4, out);
}

static void build_rgb(SleerPaletteData *out)
{
    sleer_palette_rgb_bits(1, out);
}

static void build_cmyk(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {255, 255, 255},
        {0, 255, 255},
        {255, 0, 255},
        {255, 255, 0},
        {0, 0, 0},
    };

    palette_from_list(out, colors, 5);
}

static void build_3bit(SleerPaletteData *out)
{
    sleer_palette_rgb_bits(3, out);
}

static void build_6bit(SleerPaletteData *out)
{
    sleer_palette_rgb_bits(2, out);
}

static void build_gameboy(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {15, 56, 15},
        {48, 98, 48},
        {139, 172, 15},
        {155, 188, 15},
    };

    palette_from_list(out, colors, 4);
}

static void build_teletext(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},       {255, 0, 0},   {0, 255, 0},   {0, 0, 255},
        {255, 255, 0},   {255, 0, 255}, {0, 255, 255}, {255, 255, 255},
    };

    palette_from_list(out, colors, 8);
}

static void build_apple2(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},      {255, 255, 255}, {157, 75, 154}, {44, 140, 44},
        {240, 120, 0},  {60, 90, 220},
    };

    palette_from_list(out, colors, 6);
}

static void build_c64(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},       {255, 255, 255}, {136, 0, 0},     {170, 255, 238},
        {204, 68, 204},  {0, 204, 85},    {0, 0, 170},     {238, 238, 119},
        {221, 136, 85},  {102, 68, 0},    {255, 119, 119}, {51, 51, 51},
        {119, 119, 119}, {170, 255, 102}, {0, 136, 255},   {187, 187, 187},
    };

    palette_from_list(out, colors, 16);
}

static void build_zx_spectrum(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},       {0, 0, 215},     {215, 0, 0},     {215, 0, 215},
        {0, 215, 0},     {0, 215, 215},   {215, 215, 0},   {215, 215, 215},
        {0, 0, 255},     {255, 0, 0},     {255, 0, 255},   {0, 255, 0},
        {0, 255, 255},   {255, 255, 0},   {255, 255, 255},
    };

    palette_from_list(out, colors, 15);
}

static void build_vaporwave(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {27, 11, 46},   {61, 28, 92},   {123, 47, 247},
        {255, 110, 199}, {0, 229, 255}, {255, 255, 102},
    };

    palette_from_list(out, colors, 6);
}

static void build_hacker(SleerPaletteData *out)
{
    static const SleerColor colors[] = {
        {0, 0, 0},
        {0, 43, 0},
        {0, 143, 17},
        {0, 255, 65},
    };

    palette_from_list(out, colors, 4);
}

typedef struct {
    const char *name;
    const char *id;
    void (*build)(SleerPaletteData *out);
    SleerDitherType type;
    SleerKernel kernel;
} PresetDefinition;

static const PresetDefinition k_presets[] = {
    {"Black & White", "black-white", build_black_white, SLEER_TYPE_DIFFUSION,
     SLEER_KERNEL_FLOYD_STEINBERG},
    {"2-Bit Gray", "gray-2bit", build_gray_2bit, SLEER_TYPE_DIFFUSION,
     SLEER_KERNEL_FLOYD_STEINBERG},
    {"RGB", "rgb", build_rgb, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"CMYK", "cmyk", build_cmyk, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"3-Bit", "rgb-3bit", build_3bit, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"6-Bit RGB", "rgb-6bit", build_6bit, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"Game Boy", "gameboy", build_gameboy, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"Teletext", "teletext", build_teletext, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_ATKINSON},
    {"Apple II", "apple2", build_apple2, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"Commodore 64", "c64", build_c64, SLEER_TYPE_DIFFUSION, SLEER_KERNEL_FLOYD_STEINBERG},
    {"ZX Spectrum", "zx-spectrum", build_zx_spectrum, SLEER_TYPE_DIFFUSION,
     SLEER_KERNEL_SIERRA2},
    {"Vaporwave", "vaporwave", build_vaporwave, SLEER_TYPE_DIFFUSION,
     SLEER_KERNEL_FLOYD_STEINBERG},
    {"Hacker", "hacker", build_hacker, SLEER_TYPE_ORDERED, SLEER_KERNEL_FLOYD_STEINBERG},
};

#define PRESET_COUNT ((int)(sizeof(k_presets) / sizeof(k_presets[0])))

int sleer_preset_count(void)
{
    return PRESET_COUNT;
}

static SleerPreset g_preset_cache[PRESET_COUNT];
static int g_preset_cache_ready;

const SleerPreset *sleer_preset_at(int index)
{
    int i;

    if (index < 0 || index >= PRESET_COUNT) {
        return NULL;
    }
    if (!g_preset_cache_ready) {
        for (i = 0; i < PRESET_COUNT; i++) {
            g_preset_cache[i].name = k_presets[i].name;
            g_preset_cache[i].id = k_presets[i].id;
            g_preset_cache[i].type = k_presets[i].type;
            g_preset_cache[i].kernel = k_presets[i].kernel;
        }
        g_preset_cache_ready = 1;
    }
    return &g_preset_cache[index];
}

int sleer_preset_find(const char *id_or_name)
{
    int i;

    if (!id_or_name) {
        return -1;
    }
    for (i = 0; i < PRESET_COUNT; i++) {
        if (strcmp(id_or_name, k_presets[i].id) == 0 ||
            strcmp(id_or_name, k_presets[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

void sleer_preset_apply(int index, SleerPaletteData *palette, SleerParams *params)
{
    if (index < 0 || index >= PRESET_COUNT) {
        return;
    }

    if (palette) {
        k_presets[index].build(palette);
    }
    if (params) {
        sleer_params_defaults(params);
        params->type = k_presets[index].type;
        params->kernel = k_presets[index].kernel;
    }
}

/* ------------------------------------------------------------ palette edits */

int sleer_palette_find(const SleerPaletteData *palette, SleerColor color)
{
    int i;

    if (!palette) {
        return -1;
    }
    for (i = 0; i < palette->count; i++) {
        if (palette->colors[i].r == color.r && palette->colors[i].g == color.g &&
            palette->colors[i].b == color.b) {
            return i;
        }
    }
    return -1;
}

int sleer_palette_add(SleerPaletteData *palette, SleerColor color)
{
    if (!palette || palette->count >= SLEER_MAX_PALETTE_COLORS) {
        return -1;
    }
    palette->colors[palette->count] = color;
    palette->count++;
    return palette->count - 1;
}

int sleer_palette_remove(SleerPaletteData *palette, int index)
{
    int i;

    if (!palette || index < 0 || index >= palette->count ||
        palette->count <= SLEER_MIN_PALETTE_COLORS) {
        return -1;
    }
    for (i = index; i < palette->count - 1; i++) {
        palette->colors[i] = palette->colors[i + 1];
    }
    palette->count--;
    return 0;
}

/* ------------------------------------------------------- median cut (image) */

enum {
    HIST_BITS = 5,
    HIST_LEVELS = 1 << HIST_BITS,
    HIST_SIZE = HIST_LEVELS * HIST_LEVELS * HIST_LEVELS
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint32_t count;
} HistEntry;

typedef struct {
    int start;
    int end; /* exclusive */
} HistBox;

static int g_sort_channel;

static int compare_hist(const void *left, const void *right)
{
    const HistEntry *a = (const HistEntry *)left;
    const HistEntry *b = (const HistEntry *)right;
    const int av = g_sort_channel == 0 ? a->r : (g_sort_channel == 1 ? a->g : a->b);
    const int bv = g_sort_channel == 0 ? b->r : (g_sort_channel == 1 ? b->g : b->b);

    return av - bv;
}

static int box_range(const HistEntry *entries, HistBox box, int channel)
{
    int min = 255;
    int max = 0;
    int i;

    for (i = box.start; i < box.end; i++) {
        const int value = channel == 0 ? entries[i].r : (channel == 1 ? entries[i].g : entries[i].b);
        if (value < min) {
            min = value;
        }
        if (value > max) {
            max = value;
        }
    }
    return max - min;
}

static int box_weight(const HistEntry *entries, HistBox box)
{
    uint32_t total = 0;
    int i;

    for (i = box.start; i < box.end; i++) {
        total += entries[i].count;
    }
    return (int)(total > 0x7fffffff ? 0x7fffffff : total);
}

int sleer_palette_extract(const uint8_t *rgba, int width, int height, int max_colors,
                          SleerPaletteData *out)
{
    uint32_t *histogram;
    HistEntry *entries;
    HistBox *boxes;
    int entry_count = 0;
    int box_count = 1;
    size_t i;
    int index, x, y, b;

    if (!rgba || !out || width <= 0 || height <= 0) {
        return -1;
    }
    if (max_colors < SLEER_MIN_PALETTE_COLORS) {
        max_colors = SLEER_MIN_PALETTE_COLORS;
    }
    if (max_colors > SLEER_MAX_PALETTE_COLORS) {
        max_colors = SLEER_MAX_PALETTE_COLORS;
    }

    histogram = (uint32_t *)calloc(HIST_SIZE, sizeof(uint32_t));
    entries = (HistEntry *)calloc(HIST_SIZE, sizeof(HistEntry));
    boxes = (HistBox *)calloc((size_t)max_colors, sizeof(HistBox));
    if (!histogram || !entries || !boxes) {
        free(histogram);
        free(entries);
        free(boxes);
        return -1;
    }

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const size_t pixel = ((size_t)y * (size_t)width + (size_t)x) * 4;
            const int r = rgba[pixel + 0] >> (8 - HIST_BITS);
            const int g = rgba[pixel + 1] >> (8 - HIST_BITS);
            const int b = rgba[pixel + 2] >> (8 - HIST_BITS);
            histogram[(r << (HIST_BITS * 2)) | (g << HIST_BITS) | b]++;
        }
    }

    for (i = 0; i < HIST_SIZE; i++) {
        if (histogram[i] == 0) {
            continue;
        }
        entries[entry_count].r = (uint8_t)(((i >> (HIST_BITS * 2)) & (HIST_LEVELS - 1)) * 8 + 4);
        entries[entry_count].g = (uint8_t)(((i >> HIST_BITS) & (HIST_LEVELS - 1)) * 8 + 4);
        entries[entry_count].b = (uint8_t)((i & (HIST_LEVELS - 1)) * 8 + 4);
        entries[entry_count].count = histogram[i];
        entry_count++;
    }

    free(histogram);

    if (entry_count == 0) {
        free(entries);
        free(boxes);
        return -1;
    }

    boxes[0].start = 0;
    boxes[0].end = entry_count;

    while (box_count < max_colors) {
        int best_box = -1;
        int best_range = 0;
        int best_channel = 0;
        int median_target;
        int split;
        int accumulated = 0;
        uint32_t total_weight;

        for (b = 0; b < box_count; b++) {
            int channel;

            if (boxes[b].end - boxes[b].start < 2) {
                continue;
            }
            for (channel = 0; channel < 3; channel++) {
                const int range = box_range(entries, boxes[b], channel);
                if (range > best_range) {
                    best_range = range;
                    best_box = b;
                    best_channel = channel;
                }
            }
        }

        if (best_box < 0 || best_range == 0) {
            break; /* every remaining box is a single colour */
        }

        g_sort_channel = best_channel;
        qsort(entries + boxes[best_box].start, (size_t)(boxes[best_box].end - boxes[best_box].start),
              sizeof(HistEntry), compare_hist);

        total_weight = (uint32_t)box_weight(entries, boxes[best_box]);
        median_target = (int)(total_weight / 2);
        split = boxes[best_box].start;
        for (index = boxes[best_box].start; index < boxes[best_box].end - 1; index++) {
            accumulated += (int)entries[index].count;
            split = index + 1;
            if (accumulated >= median_target) {
                break;
            }
        }

        boxes[box_count].start = split;
        boxes[box_count].end = boxes[best_box].end;
        boxes[best_box].end = split;
        box_count++;
    }

    out->count = 0;
    for (b = 0; b < box_count; b++) {
        uint64_t r_sum = 0;
        uint64_t g_sum = 0;
        uint64_t b_sum = 0;
        uint64_t weight = 0;

        for (index = boxes[b].start; index < boxes[b].end; index++) {
            const uint64_t count = entries[index].count;
            r_sum += (uint64_t)entries[index].r * count;
            g_sum += (uint64_t)entries[index].g * count;
            b_sum += (uint64_t)entries[index].b * count;
            weight += count;
        }
        if (weight == 0) {
            continue;
        }
        out->colors[out->count] =
            rgb((uint8_t)(r_sum / weight), (uint8_t)(g_sum / weight), (uint8_t)(b_sum / weight));
        out->count++;
    }

    free(entries);
    free(boxes);

    /* A flat image collapses to one colour; pair it with black or white so the
     * palette still has something to dither against. */
    if (out->count == 1) {
        const SleerColor only = out->colors[0];
        const int luma = (only.r * 54 + only.g * 183 + only.b * 19) >> 8;

        out->colors[1] = luma > 128 ? rgb(0, 0, 0) : rgb(255, 255, 255);
        out->count = 2;
    }
    return out->count;
}

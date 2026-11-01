/*
 * SLEER - palettes and presets.
 *
 * A palette is a small array of colours; presets pair one with sensible
 * dithering settings. Everything here is plain C so the CLI and the app share
 * it.
 */
#ifndef SLEER_PALETTE_H
#define SLEER_PALETTE_H

#include <stdint.h>

#include "dither.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SleerColor colors[SLEER_MAX_PALETTE_COLORS];
    int count;
} SleerPaletteData;

typedef struct {
    const char *name;
    const char *id;
    SleerDitherType type;
    SleerKernel kernel;
} SleerPreset;

int sleer_preset_count(void);
const SleerPreset *sleer_preset_at(int index);
int sleer_preset_find(const char *id_or_name);

/* Replaces the palette and resets the dithering settings of that preset. */
void sleer_preset_apply(int index, SleerPaletteData *palette, SleerParams *params);

/* Evenly spaced gray ramp with `levels` steps (2 .. 256). */
void sleer_palette_gray(int levels, SleerPaletteData *out);

/* `bits` per channel RGB cube (1 .. 4 bits -> 8 .. 4096 colours, capped). */
void sleer_palette_rgb_bits(int bits, SleerPaletteData *out);

/*
 * Median-cut palette extraction from an RGBA buffer.
 * Returns the number of colours written, or -1 on invalid arguments.
 */
int sleer_palette_extract(const uint8_t *rgba, int width, int height, int max_colors,
                          SleerPaletteData *out);

int sleer_palette_add(SleerPaletteData *palette, SleerColor color);
int sleer_palette_remove(SleerPaletteData *palette, int index);
int sleer_palette_find(const SleerPaletteData *palette, SleerColor color);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_PALETTE_H */

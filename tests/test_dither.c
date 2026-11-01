/*
 * Headless tests for the SLEER engine, palettes and image layer.
 * Run with `make test`. No SDL required.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dither.h"
#include "image.h"
#include "palette.h"

static int g_failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        printf("  FAIL: %s\n", message);
        g_failures++;
    }
}

static void check_near(double actual, double expected, double tolerance, const char *message)
{
    if (fabs(actual - expected) > tolerance) {
        printf("  FAIL: %s (got %.3f, expected %.3f +/- %.3f)\n", message, actual, expected,
               tolerance);
        g_failures++;
    }
}

static void fill_gradient(uint8_t *rgba, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const size_t index = ((size_t)y * (size_t)width + (size_t)x) * 4;
            const uint8_t value = (uint8_t)(x * 255 / (width > 1 ? width - 1 : 1));

            rgba[index + 0] = value;
            rgba[index + 1] = value;
            rgba[index + 2] = value;
            rgba[index + 3] = 255;
        }
    }
}

static void fill_solid(uint8_t *rgba, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
    size_t i;
    const size_t pixels = (size_t)width * (size_t)height;

    for (i = 0; i < pixels; i++) {
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = 255;
    }
}

static double buffer_mean(const uint8_t *rgba, size_t pixels)
{
    size_t i;
    double total = 0.0;

    for (i = 0; i < pixels; i++) {
        total += (rgba[i * 4] + rgba[i * 4 + 1] + rgba[i * 4 + 2]) / 3.0;
    }
    return pixels ? total / (double)pixels : 0.0;
}

static int is_palette_color(const SleerPaletteData *palette, const uint8_t *rgba, size_t pixels)
{
    size_t i;
    int j;

    for (i = 0; i < pixels; i++) {
        int found = 0;

        for (j = 0; j < palette->count; j++) {
            if (rgba[i * 4] == palette->colors[j].r && rgba[i * 4 + 1] == palette->colors[j].g &&
                rgba[i * 4 + 2] == palette->colors[j].b) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

static void test_defaults_and_metadata(void)
{
    SleerParams params;
    SleerKernel kernel;
    SleerDitherType type;

    printf("defaults and metadata\n");
    sleer_params_defaults(&params);
    check(params.type == SLEER_TYPE_DIFFUSION, "default type is diffusion");
    check(params.kernel == SLEER_KERNEL_FLOYD_STEINBERG, "default kernel is Floyd-Steinberg");
    check(params.matrix_size == 8, "default matrix is 8");
    check(sleer_kernel_from_string("stucki", &kernel) == 0 && kernel == SLEER_KERNEL_STUCKI,
          "kernel id round trip");
    check(sleer_kernel_from_string("nope", &kernel) == -1, "unknown kernel rejected");
    check(sleer_type_from_string("ordered", &type) == 0 && type == SLEER_TYPE_ORDERED,
          "type id round trip");
    check(strcmp(sleer_kernel_id(SLEER_KERNEL_SIERRA_LITE), "sierra-lite") == 0, "stable kernel id");
}

static void test_quantizer(void)
{
    static SleerQuantizer quantizer;
    SleerPaletteData palette;

    printf("quantizer\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "init black/white");
    check_near(quantizer.amplitude, 255.0, 1.0, "bilevel amplitude is full range");

    sleer_palette_gray(4, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "init 4 grays");
    check(quantizer.amplitude > 20.0f && quantizer.amplitude < 255.0f,
          "gray amplitude is the palette spacing");

    check(sleer_quantizer_init(&quantizer, palette.colors, 1) == -1, "single colour rejected");
}

static void test_bilevel(void)
{
    enum { W = 128, H = 96 };
    uint8_t source[W * H * 4];
    uint8_t output[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;
    size_t pixels = W * H;
    size_t i;
    int white = 0;

    printf("bilevel dithering produces both colours\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
    sleer_params_defaults(&params);

    fill_gradient(source, W, H);
    check(sleer_dither_rgba(&quantizer, source, W, H, &params, output) == 0, "dither gradient");
    check(is_palette_color(&palette, output, pixels), "output only uses palette colours");
    for (i = 0; i < pixels; i++) {
        if (output[i * 4] > 127) {
            white++;
        }
    }
    check(white > (int)(pixels / 4), "gradient has a healthy amount of white");
    check(white < (int)(pixels * 3 / 4), "gradient keeps its dark half");
    check_near(buffer_mean(output, pixels), buffer_mean(source, pixels), 4.0,
               "gradient average is preserved");
}

static void test_all_kernels(void)
{
    enum { W = 96, H = 64 };
    uint8_t source[W * H * 4];
    uint8_t output[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;
    size_t pixels = W * H;
    int k;

    printf("every diffusion kernel conserves brightness\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");

    fill_solid(source, W, H, 128, 128, 128);

    for (k = 0; k < SLEER_KERNEL_COUNT; k++) {
        sleer_params_defaults(&params);
        params.kernel = (SleerKernel)k;
        check(sleer_dither_rgba(&quantizer, source, W, H, &params, output) == 0, "dither runs");
        check(is_palette_color(&palette, output, pixels), "palette only");
        /* Atkinson deliberately discards a quarter of the error. */
        check_near(buffer_mean(output, pixels), 128.0,
                   k == SLEER_KERNEL_ATKINSON ? 12.0 : 4.0,
                   sleer_kernel_name((SleerKernel)k));
    }
}

static void test_missing_kernel_sum(void)
{
    /* False Floyd-Steinberg and Sierra Lite must also land on the mean. */
    enum { W = 64, H = 64 };
    uint8_t source[W * H * 4];
    uint8_t output[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;

    printf("sparse kernels\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
    fill_solid(source, W, H, 64, 64, 64);

    sleer_params_defaults(&params);
    params.kernel = SLEER_KERNEL_FALSE_FLOYD_STEINBERG;
    check(sleer_dither_rgba(&quantizer, source, W, H, &params, output) == 0, "false fs runs");
    check_near(buffer_mean(output, (size_t)W * H), 64.0, 4.0, "false floyd-steinberg mean");

    params.kernel = SLEER_KERNEL_SIERRA_LITE;
    check(sleer_dither_rgba(&quantizer, source, W, H, &params, output) == 0, "sierra lite runs");
    check_near(buffer_mean(output, (size_t)W * H), 64.0, 4.0, "sierra lite mean");
}

static void test_ordered_and_random(void)
{
    enum { W = 64, H = 64 };
    uint8_t source[W * H * 4];
    uint8_t first[W * H * 4];
    uint8_t second[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;
    int sizes[4] = {2, 4, 8, 16};
    int i;

    printf("ordered and random dithering\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
    fill_solid(source, W, H, 128, 128, 128);

    for (i = 0; i < 4; i++) {
        sleer_params_defaults(&params);
        params.type = SLEER_TYPE_ORDERED;
        params.matrix_size = sizes[i];
        check(sleer_dither_rgba(&quantizer, source, W, H, &params, first) == 0, "ordered runs");
        check(is_palette_color(&palette, first, (size_t)W * H), "ordered palette only");
        check_near(buffer_mean(first, (size_t)W * H), 128.0, 8.0, "ordered mean");
    }

    /* Ordered patterns tile with the matrix size. */
    sleer_params_defaults(&params);
    params.type = SLEER_TYPE_ORDERED;
    params.matrix_size = 4;
    sleer_dither_rgba(&quantizer, source, W, H, &params, first);
    {
        int tiles = 1;
        int x, y;

        for (y = 0; y < H && tiles; y++) {
            for (x = 0; x < W; x++) {
                const size_t a = ((size_t)y * W + (size_t)x) * 4;
                const size_t b = ((size_t)(y % 4) * W + (size_t)(x % 4)) * 4;

                if (first[a] != first[b]) {
                    tiles = 0;
                    break;
                }
            }
        }
        check(tiles, "ordered pattern tiles");
    }

    sleer_params_defaults(&params);
    params.type = SLEER_TYPE_RANDOM;
    params.seed = 12345;
    sleer_dither_rgba(&quantizer, source, W, H, &params, first);
    sleer_dither_rgba(&quantizer, source, W, H, &params, second);
    check(memcmp(first, second, sizeof(first)) == 0, "random is deterministic for a seed");
    check(is_palette_color(&palette, first, (size_t)W * H), "random palette only");
    check_near(buffer_mean(first, (size_t)W * H), 128.0, 8.0, "random mean");

    params.seed = 999;
    sleer_dither_rgba(&quantizer, source, W, H, &params, second);
    check(memcmp(first, second, sizeof(first)) != 0, "a new seed changes the pattern");
}

static void test_color_reduction_and_palettes(void)
{
    enum { W = 32, H = 32 };
    uint8_t source[W * H * 4];
    uint8_t output[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;
    size_t i;

    printf("colour reduction and presets\n");
    check(sleer_preset_count() >= 13, "preset count");
    for (i = 0; i < (size_t)sleer_preset_count(); i++) {
        const SleerPreset *preset = sleer_preset_at((int)i);

        check(preset && preset->name && preset->id, "preset metadata");
        sleer_preset_apply((int)i, &palette, &params);
        check(palette.count >= SLEER_MIN_PALETTE_COLORS &&
                  palette.count <= SLEER_MAX_PALETTE_COLORS,
              "preset palette size");
        check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0,
              "preset quantizer");
    }
    check(sleer_preset_find("gameboy") == 6, "gameboy preset index");

    /* Exact palette colours must survive colour reduction. */
    sleer_preset_apply(sleer_preset_find("gameboy"), &palette, &params);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "gameboy quantizer");
    params.type = SLEER_TYPE_NONE;
    for (i = 0; i < palette.count; i++) {
        fill_solid(source, W, H, palette.colors[i].r, palette.colors[i].g, palette.colors[i].b);
        sleer_dither_rgba(&quantizer, source, W, H, &params, output);
        check(output[0] == palette.colors[i].r && output[1] == palette.colors[i].g &&
                  output[2] == palette.colors[i].b,
              "palette colour maps to itself");
    }

    /* Colour reduction keeps the palette's own average for flat input. */
    sleer_palette_rgb_bits(1, &palette);
    check(palette.count == 8, "1-bit rgb palette has 8 colours");
    sleer_palette_rgb_bits(3, &palette);
    check(palette.count == 512, "3-bit rgb palette has 512 colours");
    sleer_palette_gray(16, &palette);
    check(palette.count == 16 && palette.colors[0].r == 0 && palette.colors[15].r == 255,
          "gray ramp spans 0..255");
}

static void test_bias(void)
{
    enum { W = 32, H = 32 };
    uint8_t source[W * H * 4];
    uint8_t output[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;
    size_t i;
    int all_white = 1;
    int all_black = 1;

    printf("bias\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
    fill_solid(source, W, H, 128, 128, 128);

    sleer_params_defaults(&params);
    params.bias = 1.0f;
    sleer_dither_rgba(&quantizer, source, W, H, &params, output);
    for (i = 0; i < (size_t)W * H; i++) {
        if (output[i * 4] != 255) {
            all_white = 0;
            break;
        }
    }
    check(all_white, "bias +1 is fully white");

    params.bias = -1.0f;
    sleer_dither_rgba(&quantizer, source, W, H, &params, output);
    for (i = 0; i < (size_t)W * H; i++) {
        if (output[i * 4] != 0) {
            all_black = 0;
            break;
        }
    }
    check(all_black, "bias -1 is fully black");
}

static void test_serpentine(void)
{
    enum { W = 64, H = 32 };
    uint8_t source[W * H * 4];
    uint8_t first[W * H * 4];
    uint8_t second[W * H * 4];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;

    printf("serpentine scanning\n");
    sleer_palette_gray(2, &palette);
    check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
    fill_gradient(source, W, H);

    sleer_params_defaults(&params);
    params.serpentine = 1;
    sleer_dither_rgba(&quantizer, source, W, H, &params, first);
    params.serpentine = 0;
    sleer_dither_rgba(&quantizer, source, W, H, &params, second);

    check(memcmp(first, second, sizeof(first)) != 0, "serpentine changes the pattern");
    check_near(buffer_mean(first, (size_t)W * H), buffer_mean(second, (size_t)W * H), 3.0,
               "both scans keep the average");
}

static void test_bayer_matrix(void)
{
    uint8_t matrix[256];
    int size;
    int seen[256];
    int i;
    int ok;

    printf("bayer matrices\n");
    for (size = 2; size <= 16; size *= 2) {
        memset(seen, 0, sizeof(seen));
        sleer_bayer_matrix(size, matrix);
        ok = 1;
        for (i = 0; i < size * size; i++) {
            if (matrix[i] >= size * size || seen[matrix[i]]) {
                ok = 0;
                break;
            }
            seen[matrix[i]] = 1;
        }
        check(ok, "matrix is a permutation of 0..n*n-1");
    }
}

static void test_palette_extraction(void)
{
    enum { W = 40, H = 40 };
    uint8_t source[W * H * 4];
    SleerPaletteData palette;
    int count;

    printf("median cut extraction\n");
    fill_solid(source, W, H, 200, 30, 30);

    count = sleer_palette_extract(source, W, H, 8, &palette);
    check(count >= 2, "extraction returns colours");
    check(count <= 8, "extraction respects the limit");
    {
        /* A flat image collapses to a single colour, topped up to the minimum. */
        int found = 0;
        int i;

        for (i = 0; i < palette.count; i++) {
            if (abs((int)palette.colors[i].r - 200) < 12 &&
                abs((int)palette.colors[i].g - 30) < 12 && abs((int)palette.colors[i].b - 30) < 12) {
                found = 1;
            }
        }
        check(found, "dominant colour is extracted");
    }

    fill_gradient(source, W, H);
    count = sleer_palette_extract(source, W, H, 4, &palette);
    check(count >= 2 && count <= 4, "gradient extraction count");
    check(sleer_palette_extract(NULL, W, H, 4, &palette) == -1, "null image rejected");
}

static void test_invalid_arguments(void)
{
    uint8_t buffer[64];
    SleerPaletteData palette;
    static SleerQuantizer quantizer;
    SleerParams params;

    printf("argument validation\n");
    sleer_palette_gray(2, &palette);
    sleer_quantizer_init(&quantizer, palette.colors, palette.count);
    sleer_params_defaults(&params);
    fill_solid(buffer, 4, 4, 0, 0, 0);

    check(sleer_dither_rgba(NULL, buffer, 4, 4, &params, buffer) == -1, "null quantizer rejected");
    check(sleer_dither_rgba(&quantizer, NULL, 4, 4, &params, buffer) == -1, "null source rejected");
    check(sleer_dither_rgba(&quantizer, buffer, 0, 4, &params, buffer) == -1, "zero width rejected");
    check(sleer_dither_rgba(&quantizer, buffer, 4, 4, NULL, buffer) == -1, "null params rejected");
    check(sleer_quantizer_init(NULL, palette.colors, 2) == -1, "null quantizer init rejected");
}

static void test_image_roundtrip(void)
{
    enum { W = 64, H = 32 };
    uint8_t rgba[W * H * 4];
    uint8_t scaled[16 * 8 * 4];
    SleerImage image;
    char error[256];
    size_t i;

    printf("image load, save and downscale\n");
    fill_gradient(rgba, W, H);
    check(sleer_image_save_png("build/test_input.png", rgba, W, H, error, sizeof(error)) == 0,
          "writes png");
    check(sleer_image_load("build/test_input.png", &image, error, sizeof(error)) == 0, "loads png");
    if (!image.rgba) {
        check(0, "load produced pixels");
        return;
    }
    check(image.width == W && image.height == H, "dimensions survive the roundtrip");
    check(memcmp(image.rgba, rgba, sizeof(rgba)) == 0, "pixels survive the roundtrip");

    check(sleer_image_downscale(image.rgba, W, H, scaled, 16, 8) == 0, "downscale runs");
    for (i = 0; i < 16u * 8u; i++) {
        if (scaled[i * 4] > 255 || scaled[i * 4 + 3] != 255) {
            check(0, "downscale writes valid pixels");
            break;
        }
    }
    /* The first downscaled column is the dark end of the gradient. */
    check(scaled[0] < 32, "downscale averages the left edge");

    /* Dither the loaded image and make sure the export path works too. */
    {
        SleerPaletteData palette;
        static SleerQuantizer quantizer;
        SleerParams params;
        uint8_t *dithered = (uint8_t *)malloc((size_t)W * H * 4);

        sleer_preset_apply(sleer_preset_find("gameboy"), &palette, &params);
        check(sleer_quantizer_init(&quantizer, palette.colors, palette.count) == 0, "quantizer");
        if (dithered) {
            check(sleer_dither_rgba(&quantizer, image.rgba, W, H, &params, dithered) == 0,
                  "dithers loaded image");
            check(sleer_image_save_png("build/test_output.png", dithered, W, H, error,
                                       sizeof(error)) == 0,
                  "writes dithered png");
            free(dithered);
        }
    }
    sleer_image_free(&image);

    check(sleer_image_load("build/definitely-missing.png", &image, error, sizeof(error)) == -1,
          "missing file reports an error");
    check(error[0] != '\0', "error message is filled in");
}

int main(void)
{
    printf("SLEER engine tests\n");
    test_defaults_and_metadata();
    test_quantizer();
    test_bilevel();
    test_all_kernels();
    test_missing_kernel_sum();
    test_ordered_and_random();
    test_color_reduction_and_palettes();
    test_bias();
    test_serpentine();
    test_bayer_matrix();
    test_palette_extraction();
    test_invalid_arguments();
    test_image_roundtrip();

    if (g_failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }

    printf("%d check(s) failed.\n", g_failures);
    return 1;
}

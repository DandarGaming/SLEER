#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static void set_error(char *err, size_t err_size, const char *message)
{
    if (!err || err_size == 0) {
        return;
    }
    snprintf(err, err_size, "%s", message ? message : "unknown error");
}

void sleer_image_free(SleerImage *image)
{
    if (!image) {
        return;
    }
    free(image->rgba);
    image->rgba = NULL;
    image->width = 0;
    image->height = 0;
}

int sleer_image_load(const char *path, SleerImage *image, char *err, size_t err_size)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels;
    const char *reason;

    if (!path || !image) {
        set_error(err, err_size, "invalid arguments");
        return -1;
    }

    memset(image, 0, sizeof(*image));

    pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels) {
        reason = stbi_failure_reason();
        set_error(err, err_size, reason ? reason : "could not decode image");
        return -1;
    }

    if (width <= 0 || height <= 0) {
        stbi_image_free(pixels);
        set_error(err, err_size, "image has no pixels");
        return -1;
    }

    image->rgba = (uint8_t *)malloc((size_t)width * (size_t)height * 4);
    if (!image->rgba) {
        stbi_image_free(pixels);
        set_error(err, err_size, "out of memory");
        return -1;
    }

    memcpy(image->rgba, pixels, (size_t)width * (size_t)height * 4);
    stbi_image_free(pixels);

    image->width = width;
    image->height = height;
    return 0;
}

int sleer_image_save_png(const char *path, const uint8_t *rgba, int width, int height, char *err,
                         size_t err_size)
{
    uint8_t *rgb;
    size_t pixels;
    size_t i;
    int ok;

    if (!path || !rgba || width <= 0 || height <= 0) {
        set_error(err, err_size, "invalid arguments");
        return -1;
    }

    pixels = (size_t)width * (size_t)height;
    rgb = (uint8_t *)malloc(pixels * 3);
    if (!rgb) {
        set_error(err, err_size, "out of memory");
        return -1;
    }

    for (i = 0; i < pixels; i++) {
        rgb[i * 3 + 0] = rgba[i * 4 + 0];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }

    ok = stbi_write_png(path, width, height, 3, rgb, width * 3);
    free(rgb);

    if (!ok) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "could not write %s", path);
        set_error(err, err_size, buffer);
        return -1;
    }

    return 0;
}

int sleer_image_downscale(const uint8_t *src, int src_width, int src_height, uint8_t *dst,
                          int dst_width, int dst_height)
{
    int x, y;

    if (!src || !dst || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return -1;
    }

    for (y = 0; y < dst_height; y++) {
        const int y0 = (int)((int64_t)y * src_height / dst_height);
        int y1 = (int)((int64_t)(y + 1) * src_height / dst_height);

        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > src_height) {
            y1 = src_height;
        }

        for (x = 0; x < dst_width; x++) {
            const int x0 = (int)((int64_t)x * src_width / dst_width);
            int x1 = (int)((int64_t)(x + 1) * src_width / dst_width);
            uint32_t r = 0;
            uint32_t g = 0;
            uint32_t b = 0;
            uint32_t count = 0;
            int sy, sx;

            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > src_width) {
                x1 = src_width;
            }

            for (sy = y0; sy < y1; sy++) {
                for (sx = x0; sx < x1; sx++) {
                    const uint8_t *pixel = src + ((size_t)sy * (size_t)src_width + (size_t)sx) * 4;
                    r += pixel[0];
                    g += pixel[1];
                    b += pixel[2];
                    count++;
                }
            }

            if (count == 0) {
                count = 1;
            }

            {
                uint8_t *target = dst + ((size_t)y * (size_t)dst_width + (size_t)x) * 4;
                target[0] = (uint8_t)(r / count);
                target[1] = (uint8_t)(g / count);
                target[2] = (uint8_t)(b / count);
                target[3] = 255;
            }
        }
    }

    return 0;
}

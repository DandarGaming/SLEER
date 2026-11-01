#ifndef SLEER_IMAGE_H
#define SLEER_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;
    int height;
    uint8_t *rgba; /* width * height * 4, 8 bits per channel */
} SleerImage;

/* Loads PNG, JPEG, BMP, TGA, GIF, PSD, HDR or PNM. Returns 0 on success. */
int sleer_image_load(const char *path, SleerImage *image, char *err, size_t err_size);

void sleer_image_free(SleerImage *image);

/* Writes an RGBA buffer as an RGB PNG (alpha is dropped). */
int sleer_image_save_png(const char *path, const uint8_t *rgba, int width, int height, char *err,
                         size_t err_size);

/* Box-filter downscale of an RGBA buffer, used for the fast preview proxy. */
int sleer_image_downscale(const uint8_t *src, int src_width, int src_height, uint8_t *dst,
                          int dst_width, int dst_height);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_IMAGE_H */

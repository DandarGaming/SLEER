/*
 * SLEER - background dithering worker.
 *
 * The UI thread queues a request and keeps drawing; the worker dithers the
 * newest request and publishes the result. While a slider is moving the app
 * asks for a downscaled proxy, and once the user stops it asks for the full
 * resolution pass.
 */
#ifndef SLEER_WORKER_H
#define SLEER_WORKER_H

#include <stdbool.h>
#include <stdint.h>

#include "dither.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DitherWorker DitherWorker;

DitherWorker *worker_create(void);
void worker_destroy(DitherWorker *worker);

/*
 * Replaces the source image. The buffers are copied, so the caller may free
 * them immediately afterwards. Blocks until the worker is idle.
 * `proxy` may be NULL when the image is already small.
 */
void worker_set_image(DitherWorker *worker, const uint8_t *full, int full_width, int full_height,
                      const uint8_t *proxy, int proxy_width, int proxy_height);

/* Replaces the palette; rebuilds the nearest-colour table on the worker. */
void worker_set_palette(DitherWorker *worker, const SleerColor *colors, int count);

/* Queues a dither pass with the given settings. */
void worker_request(DitherWorker *worker, const SleerParams *params, bool full_resolution);

/* Generation of the newest queued request; results carry the same number. */
uint64_t worker_generation(DitherWorker *worker);

/*
 * Locks and inspects the latest result. The pixels stay valid until
 * worker_unlock_result(). Returns false if nothing has been produced yet.
 */
bool worker_lock_result(DitherWorker *worker, const uint8_t **pixels, int *width, int *height,
                        uint64_t *generation, double *milliseconds);
void worker_unlock_result(DitherWorker *worker);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_WORKER_H */

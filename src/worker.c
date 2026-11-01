#include "worker.h"

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

struct DitherWorker {
    SDL_Mutex *mutex;
    SDL_Condition *work_available;
    SDL_Condition *idle;
    SDL_Thread *thread;

    bool quit;
    bool busy;

    uint64_t requested;
    uint64_t completed;
    SleerParams params;
    bool want_full;

    uint8_t *full;
    int full_width;
    int full_height;
    uint8_t *proxy;
    int proxy_width;
    int proxy_height;

    SleerColor palette[SLEER_MAX_PALETTE_COLORS];
    int palette_count;
    uint32_t palette_version;
    uint32_t palette_built;

    SleerQuantizer *quantizer;

    uint8_t *result;
    int result_width;
    int result_height;
    uint64_t result_generation;
    double result_milliseconds;
};

static uint8_t *copy_pixels(const uint8_t *pixels, int width, int height)
{
    uint8_t *copy;

    if (!pixels || width <= 0 || height <= 0) {
        return NULL;
    }
    copy = (uint8_t *)malloc((size_t)width * (size_t)height * 4);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, pixels, (size_t)width * (size_t)height * 4);
    return copy;
}

static int SDLCALL worker_thread(void *data)
{
    DitherWorker *worker = (DitherWorker *)data;
    uint8_t *scratch = NULL;

    for (;;) {
        const uint8_t *source;
        int width;
        int height;
        SleerParams params;
        uint64_t generation;
        double milliseconds = 0.0;

        SDL_LockMutex(worker->mutex);
        while (!worker->quit && worker->requested <= worker->completed) {
            SDL_WaitCondition(worker->work_available, worker->mutex);
        }
        if (worker->quit) {
            SDL_UnlockMutex(worker->mutex);
            break;
        }

        worker->busy = true;
        generation = worker->requested;
        params = worker->params;

        if (worker->want_full || !worker->proxy) {
            source = worker->full;
            width = worker->full_width;
            height = worker->full_height;
        } else {
            source = worker->proxy;
            width = worker->proxy_width;
            height = worker->proxy_height;
        }

        if (worker->palette_built != worker->palette_version) {
            if (sleer_quantizer_init(worker->quantizer, worker->palette, worker->palette_count) == 0) {
                worker->palette_built = worker->palette_version;
            }
        }
        SDL_UnlockMutex(worker->mutex);

        if (source && width > 0 && height > 0) {
            const size_t bytes = (size_t)width * (size_t)height * 4;
            const Uint64 start = SDL_GetPerformanceCounter();
            const Uint64 frequency = SDL_GetPerformanceFrequency();

            free(scratch);
            scratch = (uint8_t *)malloc(bytes);
            if (scratch && sleer_dither_rgba(worker->quantizer, source, width, height, &params,
                                             scratch) != 0) {
                free(scratch);
                scratch = NULL;
            }
            if (scratch) {
                milliseconds = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                               (double)frequency;
            }
        }

        SDL_LockMutex(worker->mutex);
        free(worker->result);
        worker->result = scratch;
        worker->result_width = width;
        worker->result_height = height;
        worker->result_generation = generation;
        worker->result_milliseconds = milliseconds;
        worker->completed = generation;
        worker->busy = false;
        SDL_BroadcastCondition(worker->idle);
        SDL_UnlockMutex(worker->mutex);

        scratch = NULL;
    }

    free(scratch);
    return 0;
}

DitherWorker *worker_create(void)
{
    DitherWorker *worker = (DitherWorker *)calloc(1, sizeof(DitherWorker));

    if (!worker) {
        return NULL;
    }

    worker->quantizer = (SleerQuantizer *)calloc(1, sizeof(SleerQuantizer));
    worker->mutex = SDL_CreateMutex();
    worker->work_available = SDL_CreateCondition();
    worker->idle = SDL_CreateCondition();
    if (!worker->quantizer || !worker->mutex || !worker->work_available || !worker->idle) {
        worker_destroy(worker);
        return NULL;
    }

    sleer_params_defaults(&worker->params);

    worker->thread = SDL_CreateThread(worker_thread, "sleer-dither", worker);
    if (!worker->thread) {
        worker_destroy(worker);
        return NULL;
    }

    return worker;
}

void worker_destroy(DitherWorker *worker)
{
    uint8_t *full;
    uint8_t *proxy;
    uint8_t *result;

    if (!worker) {
        return;
    }

    if (worker->thread) {
        SDL_LockMutex(worker->mutex);
        worker->quit = true;
        SDL_BroadcastCondition(worker->work_available);
        SDL_BroadcastCondition(worker->idle);
        SDL_UnlockMutex(worker->mutex);
        SDL_WaitThread(worker->thread, NULL);
    }

    full = worker->full;
    proxy = worker->proxy;
    result = worker->result;
    free(full);
    free(proxy);
    free(result);
    free(worker->quantizer);

    SDL_DestroyCondition(worker->idle);
    SDL_DestroyCondition(worker->work_available);
    SDL_DestroyMutex(worker->mutex);
    free(worker);
}

void worker_set_image(DitherWorker *worker, const uint8_t *full, int full_width, int full_height,
                      const uint8_t *proxy, int proxy_width, int proxy_height)
{
    uint8_t *new_full;
    uint8_t *new_proxy = NULL;

    if (!worker) {
        return;
    }

    /* Copy outside the lock, then wait for the worker to finish before swapping. */
    new_full = copy_pixels(full, full_width, full_height);
    if (proxy) {
        new_proxy = copy_pixels(proxy, proxy_width, proxy_height);
    }

    SDL_LockMutex(worker->mutex);
    while (worker->busy) {
        SDL_WaitCondition(worker->idle, worker->mutex);
    }
    free(worker->full);
    free(worker->proxy);
    free(worker->result);
    worker->result = NULL;
    worker->result_generation = 0;

    worker->full = new_full;
    worker->full_width = new_full ? full_width : 0;
    worker->full_height = new_full ? full_height : 0;
    worker->proxy = new_proxy;
    worker->proxy_width = new_proxy ? proxy_width : 0;
    worker->proxy_height = new_proxy ? proxy_height : 0;

    /* Drop any queued work for the old image. */
    worker->completed = worker->requested;
    SDL_UnlockMutex(worker->mutex);
}

void worker_set_palette(DitherWorker *worker, const SleerColor *colors, int count)
{
    int i;

    if (!worker || !colors || count < SLEER_MIN_PALETTE_COLORS ||
        count > SLEER_MAX_PALETTE_COLORS) {
        return;
    }

    SDL_LockMutex(worker->mutex);
    while (worker->busy) {
        SDL_WaitCondition(worker->idle, worker->mutex);
    }
    for (i = 0; i < count; i++) {
        worker->palette[i] = colors[i];
    }
    worker->palette_count = count;
    worker->palette_version++;
    SDL_UnlockMutex(worker->mutex);
}

void worker_request(DitherWorker *worker, const SleerParams *params, bool full_resolution)
{
    if (!worker || !params) {
        return;
    }

    SDL_LockMutex(worker->mutex);
    worker->params = *params;
    worker->want_full = full_resolution;
    worker->requested++;
    SDL_BroadcastCondition(worker->work_available);
    SDL_UnlockMutex(worker->mutex);
}

uint64_t worker_generation(DitherWorker *worker)
{
    uint64_t generation;

    if (!worker) {
        return 0;
    }
    SDL_LockMutex(worker->mutex);
    generation = worker->result_generation > worker->requested ? worker->result_generation
                                                              : worker->requested;
    SDL_UnlockMutex(worker->mutex);
    return generation;
}

bool worker_lock_result(DitherWorker *worker, const uint8_t **pixels, int *width, int *height,
                        uint64_t *generation, double *milliseconds)
{
    if (!worker) {
        return false;
    }

    SDL_LockMutex(worker->mutex);
    if (!worker->result || worker->palette_built != worker->palette_version) {
        SDL_UnlockMutex(worker->mutex);
        return false;
    }

    if (pixels) {
        *pixels = worker->result;
    }
    if (width) {
        *width = worker->result_width;
    }
    if (height) {
        *height = worker->result_height;
    }
    if (generation) {
        *generation = worker->result_generation;
    }
    if (milliseconds) {
        *milliseconds = worker->result_milliseconds;
    }
    return true;
}

void worker_unlock_result(DitherWorker *worker)
{
    if (worker) {
        SDL_UnlockMutex(worker->mutex);
    }
}

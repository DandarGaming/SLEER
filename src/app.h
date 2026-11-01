#ifndef SLEER_APP_H
#define SLEER_APP_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "dither.h"
#include "image.h"
#include "palette.h"
#include "ui.h"
#include "worker.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_LEFT_WIDTH 300.0f
#define APP_RIGHT_WIDTH 268.0f
#define APP_TOP_HEIGHT 48.0f
#define APP_STATUS_HEIGHT 28.0f
#define APP_PAD 16.0f
#define APP_PATH_CHARS 1024

/* Longest side of the proxy image used while settings are changing. */
#define APP_PROXY_SIZE 900

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Ui ui;
    DitherWorker *worker;

    SleerImage image;
    uint8_t *proxy;
    int proxy_width;
    int proxy_height;

    SleerPaletteData palette;
    int preset_index;
    int selected_swatch;
    int palette_scroll;

    SleerParams params;

    bool show_original;
    float compare; /* 0 = dithered, 1 = source; the canvas splits in between */
    bool compare_dragging;

    bool splash; /* startup logo, drawn over the UI until it fades out */
    uint64_t splash_start;

    bool settings_open; /* placeholder panel behind the cog button */
    bool fit;
    float zoom;
    float pan_x;
    float pan_y;

    SDL_Texture *tex_original;
    SDL_Texture *tex_result;
    int tex_result_width;
    int tex_result_height;
    uint64_t displayed_generation;

    bool waiting_full;
    uint64_t idle_deadline;
    double dither_ms;

    float mouse_x;
    float mouse_y;
    bool mouse_down;
    bool mouse_pressed;
    bool mouse_released;
    bool dragging;
    float drag_x;
    float drag_y;

    bool running;
    char file[APP_PATH_CHARS];
    char status[512];
    char *pending_open;
    char *pending_save;
} App;

void app_status(App *app, const char *fmt, ...);

/* Queues a preview dither and schedules the full-resolution pass. */
void app_invalidate(App *app);

/* Replaces the palette and pushes it to the worker. */
void app_set_palette(App *app, const SleerPaletteData *palette, int preset_index);

void app_apply_preset(App *app, int index);

int app_load(App *app, const char *path);

/* Loads the image on the clipboard; returns 0 on success. */
int app_paste_clipboard(App *app);

void app_open_dialog(App *app);
void app_save_dialog(App *app);
void app_save_to(App *app, const char *path);

/* Runs a full-resolution dither into a freshly allocated RGBA buffer. */
uint8_t *app_render_full(App *app, int *width, int *height, double *milliseconds);

UiRect app_canvas_rect(App *app);
UiRect app_preset_rect(int index);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_APP_H */

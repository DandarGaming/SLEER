/*
 * SLEER - open source palette dithering app.
 *
 * Front end: SDL3 window with a small immediate-mode UI (ui.c, panels.c).
 * Engine:    portable C palette dithering (dither.c) with presets (palette.c).
 * Worker:    dithering runs on a background thread (worker.c); the UI asks for
 *            a downscaled proxy while settings change and full resolution once
 *            the user stops.
 */
#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "panels.h"

static const SDL_DialogFileFilter k_open_filters[] = {
    {"Images", "png;jpg;jpeg;bmp;tga;gif;psd;hdr;ppm;pgm"},
    {"All files", "*"},
};

static const SDL_DialogFileFilter k_save_filters[] = {
    {"PNG image", "png"},
};

void app_status(App *app, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    SDL_vsnprintf(app->status, sizeof(app->status), fmt, args);
    va_end(args);
}

UiRect app_canvas_rect(App *app)
{
    int width = 0;
    int height = 0;

    SDL_GetWindowSize(app->window, &width, &height);
    return ui_rect(APP_LEFT_WIDTH, APP_TOP_HEIGHT,
                   (float)width - APP_LEFT_WIDTH - APP_RIGHT_WIDTH,
                   (float)height - APP_TOP_HEIGHT - APP_STATUS_HEIGHT);
}

void app_invalidate(App *app)
{
    if (!app->worker) {
        return;
    }
    worker_request(app->worker, &app->params, false);
    app->waiting_full = true;
    app->idle_deadline = SDL_GetTicks() + 250;
}

void app_set_palette(App *app, const SleerPaletteData *palette, int preset_index)
{
    app->palette = *palette;
    app->preset_index = preset_index;
    if (app->selected_swatch >= app->palette.count) {
        app->selected_swatch = app->palette.count - 1;
    }
    if (app->selected_swatch < 0 && app->palette.count > 0) {
        app->selected_swatch = 0;
    }
    worker_set_palette(app->worker, app->palette.colors, app->palette.count);
    app_invalidate(app);
}

void app_apply_preset(App *app, int index)
{
    SleerPaletteData palette;
    const SleerPreset *preset = sleer_preset_at(index);

    if (!preset) {
        return;
    }

    sleer_preset_apply(index, &palette, &app->params);
    app->selected_swatch = 0;
    app->palette_scroll = 0;
    app_set_palette(app, &palette, index);
    app_status(app, "%s preset - %d colours", preset->name, palette.count);
}

/* --------------------------------------------------------------- textures */

static void free_textures(App *app)
{
    if (app->tex_original) {
        SDL_DestroyTexture(app->tex_original);
        app->tex_original = NULL;
    }
    if (app->tex_result) {
        SDL_DestroyTexture(app->tex_result);
        app->tex_result = NULL;
        app->tex_result_width = 0;
        app->tex_result_height = 0;
    }
}

static void update_title(App *app)
{
    char title[APP_PATH_CHARS + 64];

    if (app->image.rgba) {
        SDL_snprintf(title, sizeof(title), "SLEER - %s", app->file[0] ? app->file : "untitled");
    } else {
        SDL_snprintf(title, sizeof(title), "SLEER");
    }
    SDL_SetWindowTitle(app->window, title);
}

int app_load(App *app, const char *path)
{
    SleerImage loaded;
    char error[512];
    int longest;

    if (sleer_image_load(path, &loaded, error, sizeof(error)) != 0) {
        app_status(app, "Could not open %s: %s", path, error);
        return -1;
    }

    free_textures(app);
    sleer_image_free(&app->image);
    free(app->proxy);
    app->proxy = NULL;
    app->proxy_width = 0;
    app->proxy_height = 0;

    app->image = loaded;

    app->tex_original = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STREAMING, app->image.width,
                                          app->image.height);
    if (!app->tex_original) {
        app_status(app, "Could not create a texture for %dx%d: %s", app->image.width,
                   app->image.height, SDL_GetError());
        sleer_image_free(&app->image);
        return -1;
    }
    SDL_SetTextureScaleMode(app->tex_original, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(app->tex_original, NULL, app->image.rgba, app->image.width * 4);

    longest = app->image.width > app->image.height ? app->image.width : app->image.height;
    if (longest > APP_PROXY_SIZE) {
        const double scale = (double)APP_PROXY_SIZE / (double)longest;
        int proxy_width = (int)((double)app->image.width * scale);
        int proxy_height = (int)((double)app->image.height * scale);

        if (proxy_width < 1) {
            proxy_width = 1;
        }
        if (proxy_height < 1) {
            proxy_height = 1;
        }

        app->proxy = (uint8_t *)malloc((size_t)proxy_width * (size_t)proxy_height * 4);
        if (app->proxy &&
            sleer_image_downscale(app->image.rgba, app->image.width, app->image.height, app->proxy,
                                  proxy_width, proxy_height) == 0) {
            app->proxy_width = proxy_width;
            app->proxy_height = proxy_height;
        } else {
            free(app->proxy);
            app->proxy = NULL;
        }
    }

    worker_set_image(app->worker, app->image.rgba, app->image.width, app->image.height, app->proxy,
                     app->proxy_width, app->proxy_height);
    worker_set_palette(app->worker, app->palette.colors, app->palette.count);

    app->displayed_generation = 0;
    app->fit = true;
    app->pan_x = 0.0f;
    app->pan_y = 0.0f;
    app->show_original = false;

    SDL_snprintf(app->file, sizeof(app->file), "%s", path);
    update_title(app);
    app_invalidate(app);
    app_status(app, "Loaded %s (%dx%d)", path, app->image.width, app->image.height);
    return 0;
}

/* ------------------------------------------------------------- dithering */

uint8_t *app_render_full(App *app, int *width, int *height, double *milliseconds)
{
    SleerQuantizer *quantizer;
    uint8_t *output;
    const size_t pixels = (size_t)app->image.width * (size_t)app->image.height;
    Uint64 start;
    Uint64 frequency;

    if (!app->image.rgba || pixels == 0) {
        return NULL;
    }

    quantizer = (SleerQuantizer *)malloc(sizeof(SleerQuantizer));
    output = (uint8_t *)malloc(pixels * 4);
    if (!quantizer || !output) {
        free(quantizer);
        free(output);
        return NULL;
    }

    if (sleer_quantizer_init(quantizer, app->palette.colors, app->palette.count) != 0) {
        free(quantizer);
        free(output);
        return NULL;
    }

    start = SDL_GetPerformanceCounter();
    frequency = SDL_GetPerformanceFrequency();
    if (sleer_dither_rgba(quantizer, app->image.rgba, app->image.width, app->image.height,
                          &app->params, output) != 0) {
        free(quantizer);
        free(output);
        return NULL;
    }

    if (milliseconds) {
        *milliseconds =
            (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)frequency;
    }
    if (width) {
        *width = app->image.width;
    }
    if (height) {
        *height = app->image.height;
    }

    free(quantizer);
    return output;
}

static void upload_result(App *app)
{
    const uint8_t *pixels = NULL;
    int width = 0;
    int height = 0;
    uint64_t generation = 0;
    double milliseconds = 0.0;

    if (!worker_lock_result(app->worker, &pixels, &width, &height, &generation, &milliseconds)) {
        return;
    }

    if (pixels && width > 0 && height > 0 && generation > app->displayed_generation) {
        if (!app->tex_result || app->tex_result_width != width ||
            app->tex_result_height != height) {
            if (app->tex_result) {
                SDL_DestroyTexture(app->tex_result);
            }
            app->tex_result = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA32,
                                                SDL_TEXTUREACCESS_STREAMING, width, height);
            if (app->tex_result) {
                SDL_SetTextureScaleMode(app->tex_result, SDL_SCALEMODE_NEAREST);
                app->tex_result_width = width;
                app->tex_result_height = height;
            }
        }
        if (app->tex_result && SDL_UpdateTexture(app->tex_result, NULL, pixels, width * 4)) {
            app->displayed_generation = generation;
            app->dither_ms = milliseconds;
        }
    }

    worker_unlock_result(app->worker);
}

/* ---------------------------------------------------------------- dialogs */

static void SDLCALL on_open_dialog(void *userdata, const char *const *filelist, int filter)
{
    App *app = (App *)userdata;

    (void)filter;
    if (!filelist) {
        app_status(app, "Open failed: %s", SDL_GetError());
        return;
    }
    if (!filelist[0]) {
        return;
    }
    SDL_free(app->pending_open);
    app->pending_open = SDL_strdup(filelist[0]);
}

static void SDLCALL on_save_dialog(void *userdata, const char *const *filelist, int filter)
{
    App *app = (App *)userdata;

    (void)filter;
    if (!filelist) {
        app_status(app, "Save failed: %s", SDL_GetError());
        return;
    }
    if (!filelist[0]) {
        return;
    }
    SDL_free(app->pending_save);
    app->pending_save = SDL_strdup(filelist[0]);
}

void app_open_dialog(App *app)
{
    SDL_ShowOpenFileDialog(on_open_dialog, app, app->window, k_open_filters, 2, NULL, false);
}

void app_save_dialog(App *app)
{
    if (!app->image.rgba) {
        app_status(app, "Nothing to save - open an image first");
        return;
    }
    SDL_ShowSaveFileDialog(on_save_dialog, app, app->window, k_save_filters, 1, NULL);
}

void app_save_to(App *app, const char *path)
{
    char final_path[APP_PATH_CHARS];
    char error[512];
    uint8_t *pixels;
    int width = 0;
    int height = 0;
    double milliseconds = 0.0;
    size_t length;

    if (!app->image.rgba) {
        app_status(app, "Nothing to save - open an image first");
        return;
    }

    SDL_snprintf(final_path, sizeof(final_path), "%s", path);
    length = SDL_strlen(final_path);
    if (length < 4 || SDL_strcasecmp(final_path + length - 4, ".png") != 0) {
        SDL_strlcat(final_path, ".png", sizeof(final_path));
    }

    /* Always export a full-resolution pass, even if the preview is a proxy. */
    pixels = app_render_full(app, &width, &height, &milliseconds);
    if (!pixels) {
        app_status(app, "Could not render the full-resolution image");
        return;
    }

    if (sleer_image_save_png(final_path, pixels, width, height, error, sizeof(error)) != 0) {
        app_status(app, "Could not save: %s", error);
        free(pixels);
        return;
    }

    app_status(app, "Saved %s (%dx%d, %.1f ms)", final_path, width, height, milliseconds);
    free(pixels);
}

/* ------------------------------------------------------------------ input */

static void handle_key(App *app, const SDL_KeyboardEvent *event)
{
    const SDL_Keymod modifiers = SDL_GetModState();
    const bool ctrl = (modifiers & SDL_KMOD_CTRL) != 0;
    const UiRect canvas = app_canvas_rect(app);

    if (ctrl && event->key == SDLK_O) {
        app_open_dialog(app);
        return;
    }
    if (ctrl && event->key == SDLK_S) {
        app_save_dialog(app);
        return;
    }

    if (event->key >= SDLK_1 && event->key <= SDLK_9) {
        const int index = (int)(event->key - SDLK_1);

        if (index < SLEER_KERNEL_COUNT) {
            app->params.type = SLEER_TYPE_DIFFUSION;
            app->params.kernel = (SleerKernel)index;
            app_invalidate(app);
        }
        return;
    }

    switch (event->key) {
    case SDLK_ESCAPE:
        app->running = false;
        break;
    case SDLK_SPACE:
        app->show_original = !app->show_original;
        break;
    case SDLK_F:
        app->fit = true;
        app->pan_x = 0.0f;
        app->pan_y = 0.0f;
        break;
    case SDLK_T:
        app->params.type = (SleerDitherType)(((int)app->params.type + 1) % SLEER_TYPE_COUNT);
        app_invalidate(app);
        break;
    case SDLK_R:
        app->params.seed = (uint32_t)SDL_GetPerformanceCounter();
        app_invalidate(app);
        break;
    case SDLK_LEFTBRACKET:
        app->params.bias -= 0.05f;
        if (app->params.bias < -1.0f) {
            app->params.bias = -1.0f;
        }
        app_invalidate(app);
        break;
    case SDLK_RIGHTBRACKET:
        app->params.bias += 0.05f;
        if (app->params.bias > 1.0f) {
            app->params.bias = 1.0f;
        }
        app_invalidate(app);
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
        app->fit = false;
        app->zoom *= 1.25f;
        break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
        app->fit = false;
        app->zoom *= 0.8f;
        if (app->zoom < 0.02f) {
            app->zoom = 0.02f;
        }
        break;
    default:
        break;
    }

    (void)canvas;
}

static void poll_events(App *app)
{
    SDL_Event event;

    app->mouse_pressed = false;
    app->mouse_released = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            app->running = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                handle_key(app, &event.key);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            app->mouse_x = event.motion.x;
            app->mouse_y = event.motion.y;
            if (app->dragging) {
                app->pan_x += app->mouse_x - app->drag_x;
                app->pan_y += app->mouse_y - app->drag_y;
                app->drag_x = app->mouse_x;
                app->drag_y = app->mouse_y;
                app->fit = false;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            app->mouse_x = event.button.x;
            app->mouse_y = event.button.y;
            if (event.button.button == SDL_BUTTON_LEFT) {
                app->mouse_down = true;
                app->mouse_pressed = true;
                if (ui_rect_contains(app_canvas_rect(app), app->mouse_x, app->mouse_y) &&
                    app->image.rgba) {
                    app->dragging = true;
                    app->drag_x = app->mouse_x;
                    app->drag_y = app->mouse_y;
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            app->mouse_x = event.button.x;
            app->mouse_y = event.button.y;
            if (event.button.button == SDL_BUTTON_LEFT) {
                app->mouse_down = false;
                app->mouse_released = true;
                app->dragging = false;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            const float wheel_x = event.wheel.mouse_x;
            const float wheel_y = event.wheel.mouse_y;
            const UiRect canvas = app_canvas_rect(app);

            if (panels_scroll_palette(app, wheel_x, wheel_y, event.wheel.y)) {
                break;
            }
            if (ui_rect_contains(canvas, wheel_x, wheel_y) && app->image.rgba) {
                const float factor = event.wheel.y > 0.0f ? 1.2f : 1.0f / 1.2f;
                const float old_zoom = app->zoom;
                const float image_x =
                    (wheel_x - (canvas.x + (canvas.w - (float)app->image.width * old_zoom) * 0.5f +
                                app->pan_x)) /
                    old_zoom;
                const float image_y =
                    (wheel_y - (canvas.y + (canvas.h - (float)app->image.height * old_zoom) * 0.5f +
                                app->pan_y)) /
                    old_zoom;

                app->zoom = old_zoom * factor;
                if (app->zoom < 0.02f) {
                    app->zoom = 0.02f;
                }
                if (app->zoom > 64.0f) {
                    app->zoom = 64.0f;
                }
                app->pan_x = wheel_x - image_x * app->zoom -
                             (canvas.x + (canvas.w - (float)app->image.width * app->zoom) * 0.5f);
                app->pan_y = wheel_y - image_y * app->zoom -
                             (canvas.y + (canvas.h - (float)app->image.height * app->zoom) * 0.5f);
                app->fit = false;
            }
            break;
        }
        case SDL_EVENT_DROP_FILE:
            if (event.drop.data) {
                app_load(app, event.drop.data);
            }
            break;
        default:
            break;
        }
    }

    if (app->pending_open) {
        app_load(app, app->pending_open);
        SDL_free(app->pending_open);
        app->pending_open = NULL;
    }
    if (app->pending_save) {
        app_save_to(app, app->pending_save);
        SDL_free(app->pending_save);
        app->pending_save = NULL;
    }
}

/* ------------------------------------------------------------------ frame */

static void app_frame(App *app)
{
    int window_width = 0;
    int window_height = 0;
    int output_width = 0;
    int output_height = 0;
    float dpi = 1.0f;
    UiRect canvas;

    ui_begin_frame(&app->ui, app->mouse_x, app->mouse_y, app->mouse_down, app->mouse_pressed,
                   app->mouse_released);

    SDL_GetWindowSize(app->window, &window_width, &window_height);
    SDL_GetRenderOutputSize(app->renderer, &output_width, &output_height);
    if (window_width > 0 && output_width > 0) {
        dpi = (float)output_width / (float)window_width;
    }
    if (dpi <= 0.0f) {
        dpi = 1.0f;
    }
    app->ui.dpi = dpi;
    SDL_SetRenderScale(app->renderer, dpi, dpi);

    canvas = app_canvas_rect(app);

    upload_result(app);

    SDL_SetRenderDrawColor(app->renderer, UI_COLOR_BG.r, UI_COLOR_BG.g, UI_COLOR_BG.b, 255);
    SDL_RenderClear(app->renderer);

    panels_draw_canvas(app, canvas);
    panels_draw_left(app, (float)window_height);
    panels_draw_right(app, (float)window_width, (float)window_height);
    panels_draw_top_bar(app, (float)window_width);
    panels_draw_status(app, (float)window_width, (float)window_height);
}

/* -------------------------------------------------------------- smoke test */

static void push_mouse(SDL_EventType type, float x, float y, bool down)
{
    SDL_Event event;

    SDL_zero(event);
    event.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        event.motion.x = x;
        event.motion.y = y;
        event.motion.state = down ? SDL_BUTTON_LMASK : 0;
    } else {
        event.button.x = x;
        event.button.y = y;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.clicks = 1;
    }
    SDL_PushEvent(&event);
}

static void click(App *app, float x, float y)
{
    push_mouse(SDL_EVENT_MOUSE_MOTION, x, y, false);
    push_mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, true);
    poll_events(app);
    app_frame(app);
    SDL_RenderPresent(app->renderer);
    ui_end_frame(&app->ui);

    push_mouse(SDL_EVENT_MOUSE_MOTION, x, y, true);
    push_mouse(SDL_EVENT_MOUSE_BUTTON_UP, x, y, false);
    poll_events(app);
    app_frame(app);
    SDL_RenderPresent(app->renderer);
    ui_end_frame(&app->ui);
}

static int wait_for_generation(App *app, uint64_t target)
{
    int attempts = 0;

    while (app->displayed_generation < target && attempts < 1200) {
        poll_events(app);
        app_frame(app);
        SDL_RenderPresent(app->renderer);
        ui_end_frame(&app->ui);
        SDL_Delay(5);
        attempts++;
    }
    return app->displayed_generation >= target ? 0 : -1;
}

static int app_smoke_test(App *app, const char *input, const char *output)
{
    SDL_Surface *raw = NULL;
    SDL_Surface *shot = NULL;
    const uint8_t *pixels;
    int stride;
    int x, y;
    long dark = 0;
    long light = 0;
    char error[512];

    if (!input) {
        SDL_Log("smoke test: an input image is required");
        return 1;
    }
    if (app_load(app, input) != 0) {
        SDL_Log("smoke test: could not load %s", input);
        return 1;
    }

    app_frame(app);
    SDL_RenderPresent(app->renderer);
    ui_end_frame(&app->ui);

    /* Click the "Game Boy" preset through the regular input path. */
    {
        const int gameboy = sleer_preset_find("gameboy");
        const UiRect cell = app_preset_rect(gameboy);

        click(app, cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f);

        if (app->preset_index != gameboy) {
            SDL_Log("smoke test: clicking the Game Boy preset did not select it");
            return 1;
        }
        SDL_Log("smoke test: preset click selected %s (%d colours)",
                sleer_preset_at(gameboy)->name, app->palette.count);

        if (app->palette.count != 4 || app->palette.colors[0].r != 15 ||
            app->palette.colors[0].g != 56 || app->palette.colors[0].b != 15) {
            SDL_Log("smoke test: unexpected palette after the preset click");
            return 1;
        }

        /* Wait for the pass queued by the click, not just any older frame. */
        if (wait_for_generation(app, worker_generation(app->worker)) != 0) {
            SDL_Log("smoke test: the worker never produced a frame");
            return 1;
        }
        SDL_Log("smoke test: worker produced generation %llu in %.1f ms",
                (unsigned long long)app->displayed_generation, app->dither_ms);
        SDL_Log("smoke test: result texture %s %dx%d, zoom %.2f, canvas %.0fx%.0f",
                app->tex_result ? "ready" : "MISSING", app->tex_result_width,
                app->tex_result_height, app->zoom, app_canvas_rect(app).w,
                app_canvas_rect(app).h);
        SDL_Log("smoke test: status: %s (renderer: %s)", app->status,
                SDL_GetRendererName(app->renderer));
    }

    /* Draw one more frame and read the backbuffer before presenting it. */
    poll_events(app);
    app_frame(app);
    ui_end_frame(&app->ui);

    raw = SDL_RenderReadPixels(app->renderer, NULL);
    if (!raw) {
        SDL_Log("smoke test: readback failed: %s", SDL_GetError());
        return 1;
    }
    shot = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(raw);
    if (!shot) {
        SDL_Log("smoke test: surface conversion failed: %s", SDL_GetError());
        return 1;
    }

    pixels = (const uint8_t *)shot->pixels;
    stride = shot->pitch;

    for (y = 0; y < shot->h; y++) {
        for (x = 0; x < shot->w; x++) {
            const uint8_t *pixel = pixels + (size_t)y * (size_t)stride + (size_t)x * 4;
            const int luma = (pixel[0] * 54 + pixel[1] * 183 + pixel[2] * 19) >> 8;

            if (luma < 16) {
                dark++;
            } else if (luma > 235) {
                light++;
            }
        }
    }

    SDL_Log("smoke test: %dx%d frame, %ld dark px, %ld light px", shot->w, shot->h, dark, light);

    if (output) {
        if (shot->pitch != shot->w * 4) {
            SDL_Log("smoke test: unexpected surface pitch %d", shot->pitch);
            SDL_DestroySurface(shot);
            return 1;
        }
        if (sleer_image_save_png(output, pixels, shot->w, shot->h, error, sizeof(error)) != 0) {
            SDL_Log("smoke test: could not save %s: %s", output, error);
            SDL_DestroySurface(shot);
            return 1;
        }
        SDL_Log("smoke test: wrote %s (%dx%d)", output, shot->w, shot->h);
    }

    SDL_DestroySurface(shot);

    /* Drag the bias slider and make sure the value and a new pass follow. */
    {
        const UiRect slider = panels_bias_rect();
        const double before_bias = app->params.bias;
        const uint64_t target = worker_generation(app->worker) + 1;

        push_mouse(SDL_EVENT_MOUSE_MOTION, slider.x + slider.w * 0.5f,
                   slider.y + slider.h * 0.5f, false);
        push_mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, slider.x + slider.w * 0.5f,
                   slider.y + slider.h * 0.5f, true);
        poll_events(app);
        app_frame(app);

        push_mouse(SDL_EVENT_MOUSE_MOTION, slider.x + slider.w * 0.85f,
                   slider.y + slider.h * 0.5f, true);
        poll_events(app);
        app_frame(app);

        push_mouse(SDL_EVENT_MOUSE_BUTTON_UP, slider.x + slider.w * 0.85f,
                   slider.y + slider.h * 0.5f, false);
        poll_events(app);
        app_frame(app);
        SDL_RenderPresent(app->renderer);
        ui_end_frame(&app->ui);

        if (app->params.bias <= before_bias + 0.3) {
            SDL_Log("smoke test: dragging the bias slider did not raise the bias (%.2f -> %.2f)",
                    before_bias, (double)app->params.bias);
            return 1;
        }
        if (wait_for_generation(app, target) != 0) {
            SDL_Log("smoke test: no dither pass was queued by the slider");
            return 1;
        }
        SDL_Log("smoke test: bias slider drag worked (bias %.2f, pass %.1f ms)",
                (double)app->params.bias, app->dither_ms);

        app->params.bias = 0.0f;
        app_invalidate(app);
    }

    /* The interactive loop asks for a full-resolution pass once the user stops;
     * exercise that path here so the smoke test covers both tiers. */
    {
        worker_request(app->worker, &app->params, true);
        if (wait_for_generation(app, worker_generation(app->worker)) != 0) {
            SDL_Log("smoke test: the full-resolution pass never finished");
            return 1;
        }
        if (app->tex_result_width != app->image.width ||
            app->tex_result_height != app->image.height) {
            SDL_Log("smoke test: full-resolution texture is %dx%d, expected %dx%d",
                    app->tex_result_width, app->tex_result_height, app->image.width,
                    app->image.height);
            return 1;
        }
        SDL_Log("smoke test: full-resolution pass %dx%d in %.1f ms", app->tex_result_width,
                app->tex_result_height, app->dither_ms);
    }

    /* The save path renders full resolution synchronously; check it here. */
    {
        int width = 0;
        int height = 0;
        double milliseconds = 0.0;
        uint8_t *exported = app_render_full(app, &width, &height, &milliseconds);
        size_t i;
        int palette_only = 1;

        if (!exported) {
            SDL_Log("smoke test: app_render_full returned nothing");
            return 1;
        }
        if (width != app->image.width || height != app->image.height) {
            SDL_Log("smoke test: export size %dx%d, expected %dx%d", width, height,
                    app->image.width, app->image.height);
            free(exported);
            return 1;
        }
        for (i = 0; i < (size_t)width * (size_t)height && palette_only; i++) {
            int j;
            int found = 0;

            for (j = 0; j < app->palette.count; j++) {
                if (exported[i * 4 + 0] == app->palette.colors[j].r &&
                    exported[i * 4 + 1] == app->palette.colors[j].g &&
                    exported[i * 4 + 2] == app->palette.colors[j].b) {
                    found = 1;
                    break;
                }
            }
            palette_only = found;
        }
        free(exported);
        if (!palette_only) {
            SDL_Log("smoke test: export contains colours outside the palette");
            return 1;
        }
        SDL_Log("smoke test: export renders %dx%d in %.1f ms, palette only", width, height,
                milliseconds);
    }

    return 0;
}

/* ------------------------------------------------------------------- main */

int main(int argc, char *argv[])
{
    App app;
    int exit_code = 0;
    const char *smoke_input = NULL;
    const char *smoke_output = NULL;
    const char *initial_file = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--smoke-test") == 0) {
            smoke_input = (i + 1 < argc && argv[i + 1][0] != '-') ? argv[++i] : NULL;
            smoke_output = (i + 1 < argc && argv[i + 1][0] != '-') ? argv[++i] : NULL;
        } else if (argv[i][0] != '-') {
            initial_file = argv[i];
        }
    }

    SDL_SetAppMetadata("SLEER", "0.2.0", "io.github.dandargaming.sleer");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    memset(&app, 0, sizeof(app));
    sleer_params_defaults(&app.params);
    sleer_preset_apply(0, &app.palette, &app.params);
    app.preset_index = 0;
    app.selected_swatch = 0;
    app.running = true;
    app.zoom = 1.0f;
    app.fit = true;
    app_status(&app, "Ready - open an image to start dithering");

    app.window = SDL_CreateWindow("SLEER", 1440, 900,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!app.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    app.renderer = SDL_CreateRenderer(app.window, NULL);
    if (!app.renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(app.renderer, 1);

    ui_init(&app.ui, app.renderer, 1.0f);
    SDL_GetMouseState(&app.mouse_x, &app.mouse_y);

    app.worker = worker_create();
    if (!app.worker) {
        SDL_Log("Could not start the dither worker: %s", SDL_GetError());
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
    worker_set_palette(app.worker, app.palette.colors, app.palette.count);

    if (smoke_input || smoke_output) {
        exit_code = app_smoke_test(&app, smoke_input, smoke_output);
    } else {
        if (initial_file) {
            app_load(&app, initial_file);
        }

        while (app.running) {
            poll_events(&app);
            app_frame(&app);
            SDL_RenderPresent(app.renderer);
            ui_end_frame(&app.ui);

            if (app.waiting_full && SDL_GetTicks() >= app.idle_deadline) {
                worker_request(app.worker, &app.params, true);
                app.waiting_full = false;
            }
        }
    }

    worker_destroy(app.worker);
    free_textures(&app);
    free(app.proxy);
    sleer_image_free(&app.image);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return exit_code;
}

#include "panels.h"

#include <stdio.h>
#include <string.h>

#define SWATCH_COLS 8
#define SWATCH_CELL_W 31.0f
#define SWATCH_CELL_H 20.0f
#define SWATCH_GAP 2.0f
#define SWATCH_VISIBLE_ROWS 3

#define PRESET_ROWS ((sleer_preset_count() + 1) / 2)

static const char *const k_kernel_labels[SLEER_KERNEL_COUNT] = {
    "Floyd-Steinberg",
    "False FS",
    "Jarvis-Judice-Ninke",
    "Stucki",
    "Burkes",
    "Sierra 3",
    "Sierra 2",
    "Sierra Lite",
    "Atkinson",
};

static const char *const k_type_labels[SLEER_TYPE_COUNT] = {
    "Diffusion",
    "Ordered",
    "Random",
    "Off",
};

/* Filled in while drawing so tests can drive the same rect the user clicks. */
static UiRect g_bias_rect;

UiRect panels_bias_rect(void)
{
    return g_bias_rect;
}

UiRect app_preset_rect(int index)
{
    const float cell_width = (APP_LEFT_WIDTH - APP_PAD * 2.0f - 6.0f) * 0.5f;
    const float x = APP_PAD + (float)(index % 2) * (cell_width + 6.0f);
    const float y = APP_TOP_HEIGHT + APP_PAD + 15.0f + (float)(index / 2) * 26.0f;

    return ui_rect(x, y, cell_width, 24.0f);
}

static float swatch_top(void)
{
    return APP_TOP_HEIGHT + APP_PAD + 15.0f + (float)PRESET_ROWS * 26.0f + 24.0f;
}

static UiRect swatch_area(void)
{
    return ui_rect(APP_PAD, swatch_top(), (float)SWATCH_COLS * (SWATCH_CELL_W + SWATCH_GAP),
                   (float)SWATCH_VISIBLE_ROWS * (SWATCH_CELL_H + SWATCH_GAP));
}

static int swatch_visible_rows(const App *app)
{
    const int rows = (app->palette.count + SWATCH_COLS - 1) / SWATCH_COLS;

    return rows < SWATCH_VISIBLE_ROWS ? rows : SWATCH_VISIBLE_ROWS;
}

bool panels_scroll_palette(App *app, float x, float y, float wheel)
{
    const UiRect area = swatch_area();
    const int rows = (app->palette.count + SWATCH_COLS - 1) / SWATCH_COLS;
    int max_scroll = rows - SWATCH_VISIBLE_ROWS;

    if (!ui_rect_contains(area, x, y)) {
        return false;
    }
    if (max_scroll < 0) {
        max_scroll = 0;
    }

    app->palette_scroll -= wheel > 0.0f ? 1 : (wheel < 0.0f ? -1 : 0);
    if (app->palette_scroll < 0) {
        app->palette_scroll = 0;
    }
    if (app->palette_scroll > max_scroll) {
        app->palette_scroll = max_scroll;
    }
    return true;
}

static UiRect swatch_rect(const App *app, int index)
{
    const int row = index / SWATCH_COLS - app->palette_scroll;
    const int column = index % SWATCH_COLS;
    const UiRect area = swatch_area();

    return ui_rect(area.x + (float)column * (SWATCH_CELL_W + SWATCH_GAP),
                   area.y + (float)row * (SWATCH_CELL_H + SWATCH_GAP), SWATCH_CELL_W,
                   SWATCH_CELL_H);
}

/* ------------------------------------------------------------------- top */

void panels_draw_top_bar(App *app, float width)
{
    Ui *ui = &app->ui;
    const float button_height = 28.0f;
    const float button_y = (APP_TOP_HEIGHT - button_height) * 0.5f;
    float x = 16.0f;

    ui_fill(ui, ui_rect(0.0f, 0.0f, width, APP_TOP_HEIGHT), UI_COLOR_PANEL);
    ui_fill(ui, ui_rect(0.0f, APP_TOP_HEIGHT - 1.0f, width, 1.0f), UI_COLOR_BORDER);

    ui_text(ui, x, (APP_TOP_HEIGHT - 16.0f) * 0.5f, 2.0f, UI_COLOR_ACCENT, "SLEER");
    x += ui_text_width("SLEER", 2.0f) + 20.0f;

    {
        const char *preset = app->preset_index >= 0
                                 ? sleer_preset_at(app->preset_index)->name
                                 : "Custom palette";

        ui_text(ui, x, (APP_TOP_HEIGHT - 8.0f) * 0.5f, 1.0f, UI_COLOR_TEXT_DIM, "%s", preset);
        x += ui_text_width(preset, 1.0f) + 20.0f;
    }

    if (ui_button(ui, ui_rect(x, button_y, 76.0f, button_height), "Open", false)) {
        app_open_dialog(app);
    }
    x += 84.0f;

    if (ui_button(ui, ui_rect(x, button_y, 90.0f, button_height), "Save PNG", false)) {
        app_save_dialog(app);
    }
    x += 98.0f;

    if (ui_button(ui, ui_rect(x, button_y, 104.0f, button_height),
                  app->show_original ? "Viewing: source" : "Viewing: dither", app->show_original)) {
        app->show_original = !app->show_original;
    }
    x += 112.0f;

    if (ui_button(ui, ui_rect(x, button_y, 56.0f, button_height), "Fit", app->fit)) {
        app->fit = true;
        app->pan_x = 0.0f;
        app->pan_y = 0.0f;
    }
}

/* ---------------------------------------------------------------- canvas */

static void fit_view(App *app, UiRect canvas)
{
    float zoom_x;
    float zoom_y;

    if (!app->image.rgba || canvas.w <= 0.0f || canvas.h <= 0.0f) {
        return;
    }

    zoom_x = (canvas.w - 24.0f) / (float)app->image.width;
    zoom_y = (canvas.h - 24.0f) / (float)app->image.height;
    app->zoom = zoom_x < zoom_y ? zoom_x : zoom_y;
    app->zoom = app->zoom < 0.02f ? 0.02f : (app->zoom > 64.0f ? 64.0f : app->zoom);
    app->pan_x = 0.0f;
    app->pan_y = 0.0f;
    app->fit = true;
}

void panels_draw_canvas(App *app, UiRect canvas)
{
    Ui *ui = &app->ui;
    SDL_FRect destination;
    SDL_Texture *texture;
    float image_width;
    float image_height;

    ui_fill(ui, canvas, (SDL_Color){0x0c, 0x0d, 0x10, 0xff});

    if (!app->image.rgba || !app->tex_result) {
        const float y = canvas.y + canvas.h * 0.5f - 24.0f;

        ui_text_centered(ui, ui_rect(canvas.x, y, canvas.w, 16.0f), 1.0f, UI_COLOR_TEXT_DIM,
                         app->image.rgba ? "Dithering..." : "Open an image (Ctrl+O), or drop a file here");
        if (!app->image.rgba) {
            ui_text_centered(ui, ui_rect(canvas.x, y + 22.0f, canvas.w, 16.0f), 1.0f,
                             UI_COLOR_TEXT_DIM, "PNG, JPEG, BMP, TGA, GIF, PSD, HDR, PNM");
        }
        return;
    }

    if (app->fit) {
        fit_view(app, canvas);
    }

    image_width = (float)app->image.width * app->zoom;
    image_height = (float)app->image.height * app->zoom;

    destination.x = canvas.x + (canvas.w - image_width) * 0.5f + app->pan_x;
    destination.y = canvas.y + (canvas.h - image_height) * 0.5f + app->pan_y;
    destination.w = image_width;
    destination.h = image_height;

    texture = app->show_original ? app->tex_original : app->tex_result;

    SDL_SetRenderDrawColor(ui->renderer, 0x20, 0x21, 0x27, 0xff);
    SDL_RenderFillRect(ui->renderer, &(SDL_FRect){destination.x - 1.0f, destination.y - 1.0f,
                                                  destination.w + 2.0f, destination.h + 2.0f});
    if (!SDL_RenderTexture(ui->renderer, texture, NULL, &destination)) {
        app_status(app, "Render failed: %s", SDL_GetError());
    }
}

/* ------------------------------------------------------------ left panel */

static void draw_swatch_grid(App *app)
{
    Ui *ui = &app->ui;
    const UiRect area = swatch_area();
    const int visible_rows = swatch_visible_rows(app);
    int index;

    ui_fill(ui, area, (SDL_Color){0x14, 0x15, 0x19, 0xff});
    ui_push_clip(ui, ui_rect(area.x, area.y, area.w, (float)visible_rows * (SWATCH_CELL_H + SWATCH_GAP)));

    for (index = 0; index < app->palette.count; index++) {
        const int row = index / SWATCH_COLS - app->palette_scroll;

        if (row < 0 || row >= SWATCH_VISIBLE_ROWS) {
            continue;
        }

        {
            const UiRect cell = swatch_rect(app, index);
            const SleerColor color = app->palette.colors[index];

            ui_fill(ui, cell, (SDL_Color){color.r, color.g, color.b, 0xff});
            ui_stroke(ui, cell,
                      index == app->selected_swatch ? UI_COLOR_ACCENT : UI_COLOR_BORDER);
            if (index == app->selected_swatch) {
                ui_stroke(ui, ui_rect(cell.x + 1.0f, cell.y + 1.0f, cell.w - 2.0f, cell.h - 2.0f),
                          UI_COLOR_ACCENT);
            }

            if (ui_clicked(ui, cell)) {
                app->selected_swatch = index;
            }
        }
    }

    ui_pop_clip(ui);
}

void panels_draw_left(App *app, float window_height)
{
    Ui *ui = &app->ui;
    const float x = APP_PAD;
    const float width = APP_LEFT_WIDTH - APP_PAD * 2.0f;
    const float button_height = 24.0f;
    float y = APP_TOP_HEIGHT + APP_PAD;
    char label[96];
    int i;

    ui_fill(ui, ui_rect(0.0f, APP_TOP_HEIGHT, APP_LEFT_WIDTH,
                        window_height - APP_TOP_HEIGHT - APP_STATUS_HEIGHT),
            UI_COLOR_PANEL);
    ui_fill(ui, ui_rect(APP_LEFT_WIDTH - 1.0f, APP_TOP_HEIGHT, 1.0f,
                        window_height - APP_TOP_HEIGHT - APP_STATUS_HEIGHT),
            UI_COLOR_BORDER);

    ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "PRESETS");
    y += 15.0f;

    for (i = 0; i < sleer_preset_count(); i++) {
        const UiRect cell = app_preset_rect(i);

        if (ui_button(ui, cell, sleer_preset_at(i)->name, app->preset_index == i)) {
            app_apply_preset(app, i);
        }
    }

    y = swatch_top() - 24.0f;
    SDL_snprintf(label, sizeof(label), "PALETTE  %d COLOURS", app->palette.count);
    ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
    if (app->palette.count > SWATCH_COLS * SWATCH_VISIBLE_ROWS) {
        SDL_snprintf(label, sizeof(label), "scroll: wheel");
        ui_text(ui, x + width - ui_text_width(label, 1.0f), y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
    }

    draw_swatch_grid(app);
    y = swatch_area().y + (float)SWATCH_VISIBLE_ROWS * (SWATCH_CELL_H + SWATCH_GAP) + 12.0f;

    if (app->selected_swatch >= 0 && app->selected_swatch < app->palette.count) {
        SleerColor color = app->palette.colors[app->selected_swatch];
        const uint8_t channels[3] = {color.r, color.g, color.b};
        float value;

        SDL_snprintf(label, sizeof(label), "SWATCH %d/%d", app->selected_swatch + 1,
                     app->palette.count);
        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);

        {
            const UiRect preview = ui_rect(x + width - 40.0f, y - 4.0f, 40.0f, 16.0f);
            ui_fill(ui, preview, (SDL_Color){color.r, color.g, color.b, 0xff});
            ui_stroke(ui, preview, UI_COLOR_BORDER);
        }
        y += 18.0f;

        for (i = 0; i < 3; i++) {
            char name[2] = {(char)('R' + i), '\0'};

            ui_text(ui, x, y + 5.0f, 1.0f, UI_COLOR_TEXT_DIM, "%s  %3d", name, channels[i]);
            value = (float)channels[i];
            if (ui_slider(ui, ui_rect(x + 46.0f, y, width - 46.0f, 20.0f), &value, 0.0f, 255.0f)) {
                uint8_t *target = i == 0 ? &color.r : (i == 1 ? &color.g : &color.b);
                *target = (uint8_t)(value + 0.5f);
                app->palette.colors[app->selected_swatch] = color;
                app_set_palette(app, &app->palette, app->preset_index);
            }
            y += 26.0f;
        }
        y += 6.0f;
    } else {
        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "Click a swatch to edit it");
        y += 18.0f;
        y += 3 * 26.0f - 8.0f;
    }

    {
        const float half = (width - 6.0f) * 0.5f;

        if (ui_button(ui, ui_rect(x, y, half, button_height), "Add colour", false)) {
            SleerColor next = {128, 128, 128};

            if (app->selected_swatch >= 0 && app->selected_swatch < app->palette.count) {
                next = app->palette.colors[app->selected_swatch];
            }
            if (sleer_palette_add(&app->palette, next) >= 0) {
                app->selected_swatch = app->palette.count - 1;
                app_set_palette(app, &app->palette, -1);
            } else {
                app_status(app, "Palette is full (%d colours)", SLEER_MAX_PALETTE_COLORS);
            }
        }

        if (ui_button(ui, ui_rect(x + half + 6.0f, y, half, button_height), "Remove", false)) {
            if (sleer_palette_remove(&app->palette, app->selected_swatch) == 0) {
                if (app->selected_swatch >= app->palette.count) {
                    app->selected_swatch = app->palette.count - 1;
                }
                app_set_palette(app, &app->palette, -1);
            } else {
                app_status(app, "A palette needs at least %d colours", SLEER_MIN_PALETTE_COLORS);
            }
        }
        y += button_height + 6.0f;

        if (ui_button(ui, ui_rect(x, y, width, button_height), "Colours from image", false)) {
            SleerPaletteData extracted;
            const uint8_t *source = app->proxy ? app->proxy : app->image.rgba;
            const int source_width = app->proxy ? app->proxy_width : app->image.width;
            const int source_height = app->proxy ? app->proxy_height : app->image.height;
            const int wanted = app->palette.count < 16 ? 16 : app->palette.count;

            if (source && sleer_palette_extract(source, source_width, source_height, wanted,
                                                &extracted) > 0) {
                app->selected_swatch = 0;
                app->palette_scroll = 0;
                app_set_palette(app, &extracted, -1);
                app_status(app, "Extracted %d colours from the image", extracted.count);
            } else {
                app_status(app, "Could not extract colours - open an image first");
            }
        }
    }
}

/* ----------------------------------------------------------- right panel */

void panels_draw_right(App *app, float window_width, float window_height)
{
    Ui *ui = &app->ui;
    const float panel_x = window_width - APP_RIGHT_WIDTH;
    const float x = panel_x + APP_PAD;
    const float width = APP_RIGHT_WIDTH - APP_PAD * 2.0f;
    const float button_height = 26.0f;
    float y = APP_TOP_HEIGHT + APP_PAD;
    char label[96];
    int i;

    ui_fill(ui, ui_rect(panel_x, APP_TOP_HEIGHT, APP_RIGHT_WIDTH,
                        window_height - APP_TOP_HEIGHT - APP_STATUS_HEIGHT),
            UI_COLOR_PANEL);
    ui_fill(ui, ui_rect(panel_x, APP_TOP_HEIGHT, 1.0f,
                        window_height - APP_TOP_HEIGHT - APP_STATUS_HEIGHT),
            UI_COLOR_BORDER);

    ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "DITHERING");
    y += 15.0f;

    {
        const float half = (width - 6.0f) * 0.5f;

        for (i = 0; i < SLEER_TYPE_COUNT; i++) {
            const UiRect cell = ui_rect(x + (float)(i % 2) * (half + 6.0f),
                                        y + (float)(i / 2) * (button_height + 6.0f), half,
                                        button_height);

            if (ui_button(ui, cell, k_type_labels[i], (int)app->params.type == i)) {
                app->params.type = (SleerDitherType)i;
                app_invalidate(app);
            }
        }
        y += 2.0f * (button_height + 6.0f) + 6.0f;
    }

    if (app->params.type == SLEER_TYPE_DIFFUSION) {
        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "DIFFUSION MAP");
        y += 15.0f;

        for (i = 0; i < SLEER_KERNEL_COUNT; i++) {
            const UiRect cell = ui_rect(x, y, width, button_height - 2.0f);

            if (ui_button(ui, cell, k_kernel_labels[i], (int)app->params.kernel == i)) {
                app->params.kernel = (SleerKernel)i;
                app_invalidate(app);
            }
            y += button_height + 2.0f;
        }
        y += 8.0f;

        {
            bool serpentine = app->params.serpentine != 0;

            if (ui_toggle(ui, ui_rect(x, y, width, 20.0f), "Serpentine scan", &serpentine)) {
                app->params.serpentine = serpentine ? 1 : 0;
                app_invalidate(app);
            }
            y += 30.0f;
        }
    } else if (app->params.type == SLEER_TYPE_ORDERED) {
        const int sizes[4] = {2, 4, 8, 16};
        const float cell = (width - 18.0f) * 0.25f;

        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "MATRIX");
        y += 15.0f;
        for (i = 0; i < 4; i++) {
            SDL_snprintf(label, sizeof(label), "%d", sizes[i]);
            if (ui_button(ui, ui_rect(x + (float)i * (cell + 6.0f), y, cell, button_height), label,
                          app->params.matrix_size == sizes[i])) {
                app->params.matrix_size = sizes[i];
                app_invalidate(app);
            }
        }
        y += button_height + 20.0f;
    } else if (app->params.type == SLEER_TYPE_RANDOM) {
        if (ui_button(ui, ui_rect(x, y, width, button_height), "Reseed noise", false)) {
            app->params.seed = (uint32_t)SDL_GetPerformanceCounter();
            app->params.seed ^= app->params.seed >> 13;
            app_invalidate(app);
        }
        y += button_height + 20.0f;
    } else {
        y += 12.0f;
    }

    SDL_snprintf(label, sizeof(label), "BIAS  %+d", (int)(app->params.bias * 100.0f + 0.5f));
    ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
    y += 15.0f;
    {
        float value = app->params.bias * 100.0f;
        const UiRect slider = ui_rect(x, y, width, 22.0f);

        g_bias_rect = slider;
        if (ui_slider(ui, slider, &value, -100.0f, 100.0f)) {
            app->params.bias = value / 100.0f;
            app_invalidate(app);
        }
        y += 34.0f;
    }

    if (ui_button(ui, ui_rect(x, y, width, button_height), "Reset settings", false)) {
        sleer_params_defaults(&app->params);
        app_invalidate(app);
        app_status(app, "Dithering settings reset");
    }
    y += button_height + 16.0f;

    {
        const char *algorithm = app->params.type == SLEER_TYPE_DIFFUSION
                                    ? sleer_kernel_name(app->params.kernel)
                                    : sleer_type_name(app->params.type);

        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", algorithm);
        y += 14.0f;
        SDL_snprintf(label, sizeof(label), "%d colours", app->palette.count);
        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
        y += 14.0f;
        SDL_snprintf(label, sizeof(label), "%.1f ms", app->dither_ms);
        ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
        y += 14.0f;
        if (app->proxy) {
            SDL_snprintf(label, sizeof(label), "preview %d px", APP_PROXY_SIZE);
            ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
        }
    }
}

/* ---------------------------------------------------------------- status */

void panels_draw_status(App *app, float width, float window_height)
{
    Ui *ui = &app->ui;
    char right[256];

    ui_fill(ui, ui_rect(0.0f, window_height - APP_STATUS_HEIGHT, width, APP_STATUS_HEIGHT),
            UI_COLOR_PANEL);
    ui_fill(ui, ui_rect(0.0f, window_height - APP_STATUS_HEIGHT, width, 1.0f), UI_COLOR_BORDER);
    ui_text(ui, APP_PAD, window_height - APP_STATUS_HEIGHT + 10.0f, 1.0f, UI_COLOR_TEXT_DIM, "%s",
            app->status);

    if (app->image.rgba) {
        SDL_snprintf(right, sizeof(right), "%dx%d  |  %s  |  zoom %d%%", app->image.width,
                     app->image.height,
                     app->params.type == SLEER_TYPE_DIFFUSION ? sleer_kernel_id(app->params.kernel)
                                                             : sleer_type_id(app->params.type),
                     (int)(app->zoom * 100.0f + 0.5f));
        ui_text(ui, width - APP_PAD - ui_text_width(right, 1.0f),
                window_height - APP_STATUS_HEIGHT + 10.0f, 1.0f, UI_COLOR_TEXT_DIM, "%s", right);
    }
}

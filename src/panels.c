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

/* Canvas image rect and split from the last frame. Input is handled before a
 * frame is drawn, so the split handle is hit-tested against these values. */
static bool g_split_valid;
static UiRect g_split_image_rect;
static float g_split_x;

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

/* Pixel cog: a square rim with eight teeth, so it reads at small sizes. */
static const float k_gear_teeth[8][2] = {
    {0.0f, -1.0f},     {0.7071f, -0.7071f}, {1.0f, 0.0f},  {0.7071f, 0.7071f},
    {0.0f, 1.0f},      {-0.7071f, 0.7071f}, {-1.0f, 0.0f}, {-0.7071f, -0.7071f},
};

static void draw_gear(Ui *ui, UiRect box, SDL_Color color)
{
    const float center_x = box.x + box.w * 0.5f;
    const float center_y = box.y + box.h * 0.5f;
    const float radius = box.w * 0.32f;
    const float rim = box.w * 0.16f;
    const float tooth = box.w * 0.22f;
    int i;

    ui_fill(ui, ui_rect(center_x - radius, center_y - radius, radius * 2.0f, rim), color);
    ui_fill(ui, ui_rect(center_x - radius, center_y + radius - rim, radius * 2.0f, rim), color);
    ui_fill(ui, ui_rect(center_x - radius, center_y - radius, rim, radius * 2.0f), color);
    ui_fill(ui, ui_rect(center_x + radius - rim, center_y - radius, rim, radius * 2.0f), color);

    for (i = 0; i < 8; i++) {
        ui_fill(ui,
                ui_rect(center_x + k_gear_teeth[i][0] * radius - tooth * 0.5f,
                        center_y + k_gear_teeth[i][1] * radius - tooth * 0.5f, tooth, tooth),
                color);
    }
}

static bool gear_button(Ui *ui, UiRect rect, bool selected)
{
    const bool hovered = ui_rect_contains(rect, ui->mouse_x, ui->mouse_y);
    SDL_Color background = UI_COLOR_WIDGET;

    if (selected) {
        background = UI_COLOR_ACCENT;
    } else if (hovered) {
        background = UI_COLOR_WIDGET_HOVER;
    }

    ui_fill(ui, rect, background);
    ui_stroke(ui, rect, selected ? UI_COLOR_ACCENT : UI_COLOR_BORDER);
    draw_gear(ui,
              ui_rect(rect.x + (rect.w - 18.0f) * 0.5f, rect.y + (rect.h - 18.0f) * 0.5f, 18.0f,
                      18.0f),
              selected ? UI_COLOR_BG : (hovered ? UI_COLOR_TEXT : UI_COLOR_TEXT_DIM));
    return ui_clicked(ui, rect);
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

    if (ui_button(ui, ui_rect(x, button_y, 70.0f, button_height), "Paste", false)) {
        app_paste_clipboard(app);
    }
    x += 78.0f;

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

    /* The cog is parked here for options that do not exist yet. */
    if (gear_button(ui, ui_rect(width - APP_PAD - 32.0f, (APP_TOP_HEIGHT - 32.0f) * 0.5f,
                                32.0f, 32.0f),
                    app->settings_open)) {
        app->settings_open = !app->settings_open;
    }
}

void panels_draw_settings(App *app, float window_width, float window_height)
{
    Ui *ui = &app->ui;
    const float panel_width = 320.0f;
    const float panel_height = 164.0f;
    const UiRect panel = ui_rect(window_width - APP_RIGHT_WIDTH - panel_width - 10.0f,
                                 APP_TOP_HEIGHT + 10.0f, panel_width, panel_height);

    (void)window_height;

    if (!app->settings_open) {
        return;
    }

    ui_fill(ui, panel, UI_COLOR_PANEL);
    ui_stroke(ui, panel, UI_COLOR_BORDER);
    ui_fill(ui, ui_rect(panel.x, panel.y, panel.w, 2.0f), UI_COLOR_ACCENT);

    ui_text(ui, panel.x + 14.0f, panel.y + 16.0f, 1.5f, UI_COLOR_TEXT, "Settings");
    ui_text(ui, panel.x + 14.0f, panel.y + 46.0f, 1.0f, UI_COLOR_TEXT_DIM,
            "Room for renderer, input and palette options.");
    ui_text(ui, panel.x + 14.0f, panel.y + 64.0f, 1.0f, UI_COLOR_TEXT_DIM,
            "Nothing to configure yet.");

    ui_fill(ui, ui_rect(panel.x + 14.0f, panel.y + panel_height - 46.0f, panel.w - 28.0f, 1.0f),
            UI_COLOR_BORDER);
    ui_text(ui, panel.x + 14.0f, panel.y + panel_height - 30.0f, 1.0f, UI_COLOR_TEXT_DIM,
            "Click the cog again to close");
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

    g_split_valid = false;
    ui_fill(ui, canvas, (SDL_Color){0x0e, 0x06, 0x09, 0xff});

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

    SDL_SetRenderDrawColor(ui->renderer, 0x2b, 0x11, 0x18, 0xff);
    SDL_RenderFillRect(ui->renderer, &(SDL_FRect){destination.x - 1.0f, destination.y - 1.0f,
                                                  destination.w + 2.0f, destination.h + 2.0f});
    if (!SDL_RenderTexture(ui->renderer, texture, NULL, &destination)) {
        app_status(app, "Render failed: %s", SDL_GetError());
    }

    /* Compare slider: the source is wiped over the dithered result, and the
     * divider can be grabbed with the mouse. */
    if (!app->show_original && app->compare > 0.0f && app->tex_original) {
        const float split = app->compare > 1.0f ? 1.0f : app->compare;
        SDL_FRect source;
        SDL_FRect source_destination = destination;
        SDL_FRect divider;
        SDL_FRect handle;

        source.x = 0.0f;
        source.y = 0.0f;
        source.w = (float)app->image.width * split;
        source.h = (float)app->image.height;
        source_destination.w = destination.w * split;

        if (!SDL_RenderTexture(ui->renderer, app->tex_original, &source, &source_destination)) {
            app_status(app, "Render failed: %s", SDL_GetError());
        }

        g_split_image_rect =
            ui_rect(destination.x, destination.y, destination.w, destination.h);
        g_split_x = destination.x + destination.w * split;
        g_split_valid = true;

        SDL_SetRenderDrawColor(ui->renderer, UI_COLOR_ACCENT.r, UI_COLOR_ACCENT.g,
                               UI_COLOR_ACCENT.b, 0xff);
        divider.x = g_split_x - 1.0f;
        divider.y = destination.y;
        divider.w = 2.0f;
        divider.h = destination.h;
        SDL_RenderFillRect(ui->renderer, &divider);

        handle.w = 14.0f;
        handle.h = 34.0f;
        handle.x = g_split_x - handle.w * 0.5f;
        handle.y = destination.y + destination.h * 0.5f - handle.h * 0.5f;
        SDL_RenderFillRect(ui->renderer, &handle);

        SDL_SetRenderDrawColor(ui->renderer, UI_COLOR_BG.r, UI_COLOR_BG.g, UI_COLOR_BG.b, 0xff);
        SDL_RenderFillRect(ui->renderer,
                           &(SDL_FRect){handle.x + 5.0f, handle.y + 10.0f, 1.0f, handle.h - 20.0f});
        SDL_RenderFillRect(ui->renderer,
                           &(SDL_FRect){handle.x + 8.0f, handle.y + 10.0f, 1.0f, handle.h - 20.0f});
    }
}

bool panels_compare_grab(App *app, float x, float y)
{
    if (!g_split_valid || !app->image.rgba) {
        return false;
    }
    if (y < g_split_image_rect.y || y > g_split_image_rect.y + g_split_image_rect.h) {
        return false;
    }
    return x >= g_split_x - 12.0f && x <= g_split_x + 12.0f;
}

float panels_compare_at(App *app, float x)
{
    float value;

    if (!g_split_valid || g_split_image_rect.w <= 0.0f) {
        return app->compare;
    }
    value = (x - g_split_image_rect.x) / g_split_image_rect.w;
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

/* ------------------------------------------------------------ left panel */

static void draw_swatch_grid(App *app)
{
    Ui *ui = &app->ui;
    const UiRect area = swatch_area();
    const int visible_rows = swatch_visible_rows(app);
    int index;

    ui_fill(ui, area, (SDL_Color){0x17, 0x0b, 0x10, 0xff});
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

    SDL_snprintf(label, sizeof(label), "COMPARE  %d%% SOURCE",
                 (int)(app->compare * 100.0f + 0.5f));
    ui_text(ui, x, y, 1.0f, UI_COLOR_TEXT_DIM, "%s", label);
    y += 15.0f;
    {
        float value = app->compare * 100.0f;

        if (ui_slider(ui, ui_rect(x, y, width, 22.0f), &value, 0.0f, 100.0f)) {
            app->compare = value / 100.0f;
        }
        y += 34.0f;
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

/* ---------------------------------------------------------------- splash */

/* The wordmark is a chunky bitmap grid: every '#' is drawn as a solid block,
 * so the banner reads like CLI block art without needing a font asset. */
static const char *const k_splash_word[] = {
    "##### #.... ##### ##### #####",
    "#.... #.... #.... #.... #...#",
    "#.... #.... #.... #.... #...#",
    "##### #.... ##### ##### #####",
    "....# #.... #.... #.... #..#.",
    "....# #.... #.... #.... #...#",
    "##### ##### ##### ##### #...#",
};

#define SPLASH_CELL 22.0f
#define SPLASH_WORD_COLS 29
#define SPLASH_WORD_ROWS 7
#define SPLASH_SNAKE_COLS 33
#define SPLASH_SNAKE_CELL 9.0f
#define SPLASH_GAP 56.0f
#define SPLASH_BOLD 3.0f /* cells overlap slightly, so strokes read chunky */
#define SPLASH_HOLD_MS 900
#define SPLASH_FADE_MS 550

/* Draws the block word at (x, y); a second pass at an offset makes the shadow. */
static void draw_splash_word(Ui *ui, float x, float y, SDL_Color color)
{
    int row;
    int column;

    for (row = 0; row < SPLASH_WORD_ROWS; row++) {
        for (column = 0; column < SPLASH_WORD_COLS; column++) {
            if (k_splash_word[row][column] != '#') {
                continue;
            }
            ui_fill(ui,
                    ui_rect(x + (float)column * SPLASH_CELL - SPLASH_BOLD * 0.5f,
                            y + (float)row * SPLASH_CELL - SPLASH_BOLD * 0.5f,
                            SPLASH_CELL + SPLASH_BOLD, SPLASH_CELL + SPLASH_BOLD),
                    color);
        }
    }
}

/* Three-headed snake mascot: '#' is body, 'o' is an eye, '.' is empty. The
 * heads are diamonds, the necks taper inwards and the tail curls to a point. */
static const char *const k_splash_snake[] = {
    ".....###.......###.......###.....",
    "....#####.....#####.....#####....",
    "...##o#o##...##o#o##...##o#o##...",
    "....#####.....#####.....#####....",
    ".....###.......###.......###.....",
    ".....###.......###.......###.....",
    "......###......###......###......",
    ".......###.....###.....###.......",
    "........###....###....###........",
    "........#################........",
    "..........#############..........",
    "...........###########...........",
    "............#########............",
    ".............#######.............",
    "..............#####..............",
    "...............###...............",
    "................#................",
};

#define SPLASH_SNAKE_ROWS ((int)SDL_arraysize(k_splash_snake))

static void draw_splash_snake(Ui *ui, float x, float y, SDL_Color body, SDL_Color eye)
{
    int row;
    int column;

    for (row = 0; row < SPLASH_SNAKE_ROWS; row++) {
        for (column = 0; column < SPLASH_SNAKE_COLS; column++) {
            const char cell = k_splash_snake[row][column];

            if (cell == '.') {
                continue;
            }
            ui_fill(ui,
                    ui_rect(x + (float)column * SPLASH_SNAKE_CELL,
                            y + (float)row * SPLASH_SNAKE_CELL, SPLASH_SNAKE_CELL,
                            SPLASH_SNAKE_CELL),
                    cell == 'o' ? eye : body);
        }
    }
}

/* Corner brackets around the whole window, as on a terminal banner. */
static void draw_splash_frame(Ui *ui, float width, float height, SDL_Color color)
{
    const float margin = 22.0f;
    const float length = 46.0f;
    const float thick = 2.0f;

    ui_fill(ui, ui_rect(margin, margin, length, thick), color);
    ui_fill(ui, ui_rect(margin, margin, thick, length), color);
    ui_fill(ui, ui_rect(width - margin - length, margin, length, thick), color);
    ui_fill(ui, ui_rect(width - margin - thick, margin, thick, length), color);
    ui_fill(ui, ui_rect(margin, height - margin - thick, length, thick), color);
    ui_fill(ui, ui_rect(margin, height - margin - length, thick, length), color);
    ui_fill(ui, ui_rect(width - margin - length, height - margin - thick, length, thick), color);
    ui_fill(ui, ui_rect(width - margin - thick, height - margin - length, thick, length), color);
}

/* Fades a colour towards the window background: amount 1 keeps it, 0 hides it. */
static SDL_Color fade_to_bg(SDL_Color color, float amount)
{
    SDL_Color out;

    out.r = (Uint8)((float)UI_COLOR_BG.r + ((float)color.r - (float)UI_COLOR_BG.r) * amount);
    out.g = (Uint8)((float)UI_COLOR_BG.g + ((float)color.g - (float)UI_COLOR_BG.g) * amount);
    out.b = (Uint8)((float)UI_COLOR_BG.b + ((float)color.b - (float)UI_COLOR_BG.b) * amount);
    out.a = 0xff;
    return out;
}

void panels_draw_splash(App *app, float width, float window_height)
{
    Ui *ui = &app->ui;
    const float word_width = (float)SPLASH_WORD_COLS * SPLASH_CELL;
    const float word_height = (float)SPLASH_WORD_ROWS * SPLASH_CELL;
    const float snake_width = (float)SPLASH_SNAKE_COLS * SPLASH_SNAKE_CELL;
    const float snake_height = (float)SPLASH_SNAKE_ROWS * SPLASH_SNAKE_CELL;
    const float block_width = word_width + SPLASH_GAP + snake_width;
    const float x = (width - block_width) * 0.5f;
    const float y = window_height * 0.46f - word_height * 0.5f;
    uint64_t elapsed;
    float visible;

    if (!app->splash) {
        return;
    }

    elapsed = SDL_GetTicks() - app->splash_start;
    if (elapsed >= (uint64_t)(SPLASH_HOLD_MS + SPLASH_FADE_MS)) {
        app->splash = false;
        return;
    }
    if (elapsed <= (uint64_t)SPLASH_HOLD_MS) {
        visible = 1.0f;
    } else {
        visible = 1.0f - (float)(elapsed - SPLASH_HOLD_MS) / (float)SPLASH_FADE_MS;
    }

    /* The backdrop and the logo both fade, so the UI appears underneath. Alpha
     * only blends when the draw blend mode says so; the UI draws opaque. */
    SDL_SetRenderDrawBlendMode(ui->renderer, SDL_BLENDMODE_BLEND);
    ui_fill(ui, ui_rect(0.0f, 0.0f, width, window_height),
            (SDL_Color){UI_COLOR_BG.r, UI_COLOR_BG.g, UI_COLOR_BG.b,
                        (Uint8)(255.0f * visible)});
    SDL_SetRenderDrawBlendMode(ui->renderer, SDL_BLENDMODE_NONE);

    draw_splash_frame(ui, width, window_height, fade_to_bg(UI_COLOR_BORDER, visible));

    ui_text(ui, x, y - 34.0f, 1.25f, fade_to_bg(UI_COLOR_TEXT, visible), "Welcome to SLEER");

    draw_splash_word(ui, x + 8.0f, y + 8.0f, fade_to_bg(UI_COLOR_WIDGET_ACTIVE, visible));
    draw_splash_word(ui, x, y, fade_to_bg(UI_COLOR_ACCENT, visible));

    draw_splash_snake(ui, x + word_width + SPLASH_GAP,
                      y + (word_height - snake_height) * 0.5f,
                      fade_to_bg(UI_COLOR_SNAKE, visible), fade_to_bg(UI_COLOR_BG, visible));

    ui_text_centered(ui, ui_rect(x, y + word_height + 22.0f, word_width, 16.0f), 1.25f,
                     fade_to_bg(UI_COLOR_TEXT_DIM, visible), "image dithering  -  v0.2.0");
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

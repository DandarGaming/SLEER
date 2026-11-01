#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Crimson theme: warm near-black surfaces, a blood-red accent and a green
 * used for the splash mascot, so the snake stands apart from the UI. */
const SDL_Color UI_COLOR_BG = {0x14, 0x08, 0x0b, 0xff};
const SDL_Color UI_COLOR_PANEL = {0x21, 0x0e, 0x13, 0xff};
const SDL_Color UI_COLOR_WIDGET = {0x33, 0x16, 0x1d, 0xff};
const SDL_Color UI_COLOR_WIDGET_HOVER = {0x45, 0x1d, 0x26, 0xff};
const SDL_Color UI_COLOR_WIDGET_ACTIVE = {0x57, 0x25, 0x2f, 0xff};
const SDL_Color UI_COLOR_BORDER = {0x48, 0x1e, 0x27, 0xff};
const SDL_Color UI_COLOR_ACCENT = {0xc8, 0x24, 0x3c, 0xff};
const SDL_Color UI_COLOR_TEXT = {0xf1, 0xe3, 0xe5, 0xff};
const SDL_Color UI_COLOR_TEXT_DIM = {0xa9, 0x8a, 0x8f, 0xff};
const SDL_Color UI_COLOR_SNAKE = {0x3f, 0xb9, 0x50, 0xff};

#define UI_CHAR_W 8.0f /* SDL_RenderDebugText uses an 8x8 cell */
#define UI_CHAR_H 8.0f

UiRect ui_rect(float x, float y, float w, float h)
{
    UiRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    return rect;
}

bool ui_rect_contains(UiRect rect, float x, float y)
{
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void ui_init(Ui *ui, SDL_Renderer *renderer, float dpi)
{
    memset(ui, 0, sizeof(*ui));
    ui->renderer = renderer;
    ui->dpi = dpi > 0.0f ? dpi : 1.0f;
    SDL_SetRenderScale(renderer, ui->dpi, ui->dpi);
}

void ui_begin_frame(Ui *ui, float mouse_x, float mouse_y, bool mouse_down, bool pressed, bool released)
{
    ui->mouse_x = mouse_x;
    ui->mouse_y = mouse_y;
    ui->mouse_down = mouse_down;
    ui->pressed = pressed;
    ui->released = released;
    ui->hot_id = 0;
    ui->next_id = 0;
}

void ui_end_frame(Ui *ui)
{
    if (!ui->mouse_down) {
        ui->active_id = 0;
    }
}

void ui_fill(Ui *ui, UiRect rect, SDL_Color color)
{
    SDL_FRect r;

    r.x = rect.x;
    r.y = rect.y;
    r.w = rect.w;
    r.h = rect.h;
    SDL_SetRenderDrawColor(ui->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(ui->renderer, &r);
}

void ui_stroke(Ui *ui, UiRect rect, SDL_Color color)
{
    SDL_FRect r;

    r.x = rect.x + 0.5f;
    r.y = rect.y + 0.5f;
    r.w = rect.w - 1.0f;
    r.h = rect.h - 1.0f;
    SDL_SetRenderDrawColor(ui->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(ui->renderer, &r);
}

float ui_text_width(const char *text, float size)
{
    return text ? (float)strlen(text) * UI_CHAR_W * size : 0.0f;
}

static void draw_text(Ui *ui, float x, float y, float size, SDL_Color color, const char *text)
{
    if (!text || !*text) {
        return;
    }
    if (size < 1.0f) {
        size = 1.0f;
    }

    /* SDL's debug font has a fixed cell size; scale the renderer around it. */
    SDL_SetRenderScale(ui->renderer, ui->dpi * size, ui->dpi * size);
    SDL_SetRenderDrawColor(ui->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(ui->renderer, x / size, y / size, text);
    SDL_SetRenderScale(ui->renderer, ui->dpi, ui->dpi);
}

void ui_text(Ui *ui, float x, float y, float size, SDL_Color color, const char *fmt, ...)
{
    char buffer[512];
    va_list args;

    va_start(args, fmt);
    SDL_vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    draw_text(ui, x, y, size, color, buffer);
}

void ui_text_centered(Ui *ui, UiRect rect, float size, SDL_Color color, const char *fmt, ...)
{
    char buffer[512];
    va_list args;
    float width;

    va_start(args, fmt);
    SDL_vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    width = ui_text_width(buffer, size);
    draw_text(ui, rect.x + (rect.w - width) * 0.5f, rect.y + (rect.h - UI_CHAR_H * size) * 0.5f,
              size, color, buffer);
}

bool ui_button(Ui *ui, UiRect rect, const char *label, bool selected)
{
    const int id = ++ui->next_id;
    const bool hovered = ui_rect_contains(rect, ui->mouse_x, ui->mouse_y);
    bool clicked = false;
    SDL_Color background;

    if (ui->pressed && hovered && ui->active_id == 0) {
        ui->active_id = id;
    }
    if (ui->released && ui->active_id == id) {
        clicked = hovered;
        ui->active_id = 0;
    }
    if (hovered) {
        ui->hot_id = id;
    }

    background = selected ? UI_COLOR_ACCENT : UI_COLOR_WIDGET;
    if (hovered) {
        background = selected ? UI_COLOR_ACCENT : UI_COLOR_WIDGET_HOVER;
    }
    if (ui->active_id == id && hovered) {
        background = selected ? UI_COLOR_ACCENT : UI_COLOR_WIDGET_ACTIVE;
    }

    ui_fill(ui, rect, background);
    ui_stroke(ui, rect, selected ? UI_COLOR_ACCENT : UI_COLOR_BORDER);
    ui_text_centered(ui, rect, 1.0f, selected ? UI_COLOR_BG : UI_COLOR_TEXT, "%s", label);
    return clicked;
}

bool ui_toggle(Ui *ui, UiRect rect, const char *label, bool *value)
{
    const int id = ++ui->next_id;
    const bool hovered = ui_rect_contains(rect, ui->mouse_x, ui->mouse_y);
    bool clicked = false;
    UiRect box = ui_rect(rect.x, rect.y + (rect.h - 14.0f) * 0.5f, 14.0f, 14.0f);

    if (ui->pressed && hovered && ui->active_id == 0) {
        ui->active_id = id;
    }
    if (ui->released && ui->active_id == id) {
        clicked = hovered;
        ui->active_id = 0;
    }

    ui_fill(ui, box, *value ? UI_COLOR_ACCENT : UI_COLOR_WIDGET);
    ui_stroke(ui, box, hovered ? UI_COLOR_WIDGET_HOVER : UI_COLOR_BORDER);
    if (*value) {
        ui_fill(ui, ui_rect(box.x + 4.0f, box.y + 4.0f, 6.0f, 6.0f), UI_COLOR_BG);
    }
    ui_text(ui, box.x + 22.0f, rect.y + (rect.h - UI_CHAR_H) * 0.5f, 1.0f,
            hovered ? UI_COLOR_TEXT : UI_COLOR_TEXT_DIM, "%s", label);

    if (clicked) {
        *value = !*value;
    }
    return clicked;
}

bool ui_clicked(Ui *ui, UiRect rect)
{
    return ui->pressed && ui_rect_contains(rect, ui->mouse_x, ui->mouse_y);
}

void ui_push_clip(Ui *ui, UiRect rect)
{
    SDL_Rect clip;

    clip.x = (int)rect.x;
    clip.y = (int)rect.y;
    clip.w = (int)rect.w;
    clip.h = (int)rect.h;
    SDL_SetRenderClipRect(ui->renderer, &clip);
}

void ui_pop_clip(Ui *ui)
{
    SDL_SetRenderClipRect(ui->renderer, NULL);
}

bool ui_slider(Ui *ui, UiRect rect, float *value, float min, float max)
{
    const int id = ++ui->next_id;
    const bool hovered = ui_rect_contains(rect, ui->mouse_x, ui->mouse_y);
    const float padding = 8.0f;
    const float track_w = rect.w - padding * 2.0f;
    const float track_y = rect.y + rect.h * 0.5f;
    bool changed = false;
    float ratio;
    float knob_x;

    if (ui->pressed && hovered && ui->active_id == 0) {
        ui->active_id = id;
    }
    if (ui->active_id == id) {
        if (ui->mouse_down && track_w > 0.0f) {
            float t = (ui->mouse_x - (rect.x + padding)) / track_w;
            float next;

            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            next = min + t * (max - min);
            if (next != *value) {
                *value = next;
                changed = true;
            }
        } else {
            ui->active_id = 0;
        }
    }

    ratio = (max > min) ? (*value - min) / (max - min) : 0.0f;
    ratio = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    knob_x = rect.x + padding + track_w * ratio;

    ui_fill(ui, ui_rect(rect.x + padding, track_y - 2.0f, track_w, 4.0f), UI_COLOR_WIDGET);
    ui_fill(ui, ui_rect(rect.x + padding, track_y - 2.0f, (knob_x - rect.x - padding), 4.0f),
            UI_COLOR_ACCENT);
    ui_fill(ui, ui_rect(knob_x - 4.0f, rect.y + 3.0f, 8.0f, rect.h - 6.0f),
            (hovered || ui->active_id == id) ? UI_COLOR_TEXT : UI_COLOR_WIDGET_HOVER);

    return changed;
}

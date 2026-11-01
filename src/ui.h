/*
 * SLEER - minimal immediate-mode widgets drawn with SDL3's renderer.
 *
 * The app only needs buttons, sliders, toggles and text, so pulling in a
 * toolkit (and a font) would be overkill. Text uses SDL_RenderDebugText, which
 * ships with SDL itself.
 */
#ifndef SLEER_UI_H
#define SLEER_UI_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const SDL_Color UI_COLOR_BG;
extern const SDL_Color UI_COLOR_PANEL;
extern const SDL_Color UI_COLOR_WIDGET;
extern const SDL_Color UI_COLOR_WIDGET_HOVER;
extern const SDL_Color UI_COLOR_WIDGET_ACTIVE;
extern const SDL_Color UI_COLOR_BORDER;
extern const SDL_Color UI_COLOR_ACCENT;
extern const SDL_Color UI_COLOR_TEXT;
extern const SDL_Color UI_COLOR_TEXT_DIM;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} UiRect;

typedef struct {
    SDL_Renderer *renderer;
    float dpi; /* pixels per layout point */
    float mouse_x;
    float mouse_y;
    bool mouse_down;
    bool pressed;  /* button became pressed this frame */
    bool released; /* button was released this frame */
    int hot_id;
    int active_id;
    int next_id;
} Ui;

UiRect ui_rect(float x, float y, float w, float h);
bool ui_rect_contains(UiRect rect, float x, float y);

void ui_init(Ui *ui, SDL_Renderer *renderer, float dpi);
void ui_begin_frame(Ui *ui, float mouse_x, float mouse_y, bool mouse_down, bool pressed, bool released);
void ui_end_frame(Ui *ui);

void ui_fill(Ui *ui, UiRect rect, SDL_Color color);
void ui_stroke(Ui *ui, UiRect rect, SDL_Color color);
void ui_text(Ui *ui, float x, float y, float size, SDL_Color color, const char *fmt, ...);
void ui_text_centered(Ui *ui, UiRect rect, float size, SDL_Color color, const char *fmt, ...);
float ui_text_width(const char *text, float size);

/* `selected` draws the active state. Returns true on a completed click. */
bool ui_button(Ui *ui, UiRect rect, const char *label, bool selected);
bool ui_toggle(Ui *ui, UiRect rect, const char *label, bool *value);

/* Returns true while the value changes. */
bool ui_slider(Ui *ui, UiRect rect, float *value, float min, float max);

/* True when the primary button went down inside `rect` this frame. */
bool ui_clicked(Ui *ui, UiRect rect);

/* Clipping in layout points, for scrolling areas. */
void ui_push_clip(Ui *ui, UiRect rect);
void ui_pop_clip(Ui *ui);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_UI_H */

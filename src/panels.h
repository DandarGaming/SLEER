#ifndef SLEER_PANELS_H
#define SLEER_PANELS_H

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

void panels_draw_top_bar(App *app, float width);
void panels_draw_canvas(App *app, UiRect canvas);
void panels_draw_left(App *app, float window_height);
void panels_draw_right(App *app, float window_width, float window_height);
void panels_draw_status(App *app, float width, float window_height);

/* Placeholder panel opened by the top-bar cog. */
void panels_draw_settings(App *app, float window_width, float window_height);

/* Startup logo: holds, then fades into the program. No-op once finished. */
void panels_draw_splash(App *app, float width, float window_height);

/* Scrolls the swatch grid when the cursor is over it. Returns true if consumed. */
bool panels_scroll_palette(App *app, float x, float y, float wheel);

/* Rect of the bias slider from the most recent frame (for scripted input). */
UiRect panels_bias_rect(void);

/* True when the compare handle on the canvas is under (x, y). */
bool panels_compare_grab(App *app, float x, float y);

/* Compare value for a canvas x position, using the last drawn image rect. */
float panels_compare_at(App *app, float x);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_PANELS_H */

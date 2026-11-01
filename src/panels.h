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

/* Scrolls the swatch grid when the cursor is over it. Returns true if consumed. */
bool panels_scroll_palette(App *app, float x, float y, float wheel);

/* Rect of the bias slider from the most recent frame (for scripted input). */
UiRect panels_bias_rect(void);

#ifdef __cplusplus
}
#endif

#endif /* SLEER_PANELS_H */

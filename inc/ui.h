#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "browser.h"
#include "video.h"

typedef struct UiPalette {
  SDL_Color bg;
  SDL_Color panel;
  SDL_Color panel_shadow;
  SDL_Color panel_header;
  SDL_Color panel_header_border;

  SDL_Color list_bg;
  SDL_Color row_even;
  SDL_Color row_odd;
  SDL_Color row_selected_bg;
  SDL_Color row_selected_accent;

  SDL_Color text_primary;
  SDL_Color text_muted;
  SDL_Color text_dir;

  SDL_Color scrollbar_bg;
  SDL_Color scrollbar_handle;
} UiPalette;

typedef struct UiContext {
  SDL_Renderer *ren;
  const UiPalette *pal;
  TTF_Font *font_regular;
  TTF_Font *font_small;
} UiContext;

typedef struct {
  SDL_Rect bar;
  SDL_Rect btn_play;
  SDL_Rect btn_prev;
  SDL_Rect btn_next;
  SDL_Rect progress_bg;
  SDL_Rect vol_icon;
  SDL_Rect vol_bar;
} UiPlayerLayout;

int ui_init(UiContext *ui, SDL_Renderer *ren, const char *font_path);
void ui_shutdown(UiContext *ui);

void ui_compute_player_layout(const UiContext *ui, UiPlayerLayout *layout);
void ui_draw_video(const UiContext *ui, VideoState *vid);
void ui_draw_player_controls(const UiContext *ui, const UiPlayerLayout *layout,
                             const VideoState *vid, int paused, int muted);

void ui_draw_browser(const UiContext *ui, const FileBrowser *b);

int ui_hit_test_rect(const SDL_Rect *r, int mx, int my);
int ui_progress_hit_test(const UiPlayerLayout *layout, int mx, int my,
                         double *ratio);
int ui_volume_bar_hit_test(const UiPlayerLayout *layout, int mx, int my,
                           double *ratio);

const UiPalette *ui_palette(void);
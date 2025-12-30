#include <stdio.h>

#include "ui.h"

static const UiPalette g_palette = {
    .bg = {5, 8, 20, 255},
    .panel = {15, 23, 42, 240},
    .panel_shadow = {0, 0, 0, 120},
    .panel_header = {30, 41, 59, 255},
    .panel_header_border = {51, 65, 85, 255},

    .list_bg = {15, 23, 42, 255},
    .row_even = {15, 23, 42, 255},
    .row_odd = {17, 24, 39, 255},
    .row_selected_bg = {56, 189, 248, 50},
    .row_selected_accent = {56, 189, 248, 255},

    .text_primary = {226, 232, 240, 255},
    .text_muted = {148, 163, 184, 255},
    .text_dir = {129, 199, 212, 255},

    .scrollbar_bg = {30, 41, 59, 255},
    .scrollbar_handle = {148, 163, 184, 220},
};

const UiPalette *ui_palette(void) { return &g_palette; }

int ui_init(UiContext *ui, SDL_Renderer *ren, const char *font_path) {
  if (!ui) return 0;
  ui->ren = ren;
  ui->pal = ui_palette();
  ui->font_regular = NULL;
  ui->font_small = NULL;

  if (TTF_WasInit() == 0) {
    if (TTF_Init() != 0) {
      fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
      return 0;
    }
  }

  ui->font_regular = TTF_OpenFont(font_path, 18);
  if (!ui->font_regular) {
    fprintf(stderr, "ui: TTF_OpenFont regular failed: %s\n", TTF_GetError());
    return 0;
  }

  ui->font_small = TTF_OpenFont(font_path, 16);
  if (!ui->font_small) {
    ui->font_small = ui->font_regular;
  }

  return 1;
}

void ui_shutdown(UiContext *ui) {
  if (!ui) return;
  if (ui->font_small && ui->font_small != ui->font_regular)
    TTF_CloseFont(ui->font_small);
  if (ui->font_regular) TTF_CloseFont(ui->font_regular);
  ui->font_small = NULL;
  ui->font_regular = NULL;
  ui->ren = NULL;
  ui->pal = NULL;
}

void ui_compute_player_layout(const UiContext *ui, UiPlayerLayout *l) {
  int ww, wh;
  SDL_GetRendererOutputSize(ui->ren, &ww, &wh);

  const int ctrl_h = 72;
  const int margin = 16;

  l->bar.x = 0;
  l->bar.y = wh - ctrl_h;
  l->bar.w = ww;
  l->bar.h = ctrl_h;

  int center_y = l->bar.y + ctrl_h / 2;

  l->btn_play.w = 32;
  l->btn_play.h = 32;
  l->btn_play.x = l->bar.x + margin;
  l->btn_play.y = center_y - l->btn_play.h / 2;

  l->btn_prev.w = 28;
  l->btn_prev.h = 28;
  l->btn_prev.x = l->btn_play.x + l->btn_play.w + 12;
  l->btn_prev.y = center_y - l->btn_prev.h / 2;

  l->btn_next.w = 28;
  l->btn_next.h = 28;
  l->btn_next.x = l->btn_prev.x + l->btn_prev.w + 8;
  l->btn_next.y = center_y - l->btn_next.h / 2;

  l->vol_bar.w = 80;
  l->vol_bar.h = 6;
  l->vol_bar.x = ww - margin - l->vol_bar.w;
  l->vol_bar.y = center_y - l->vol_bar.h / 2;

  l->vol_icon.w = 24;
  l->vol_icon.h = 24;
  l->vol_icon.x = l->vol_bar.x - l->vol_icon.w - 9;
  l->vol_icon.y = center_y - l->vol_icon.h / 2;

  int progress_left = l->btn_next.x + l->btn_next.w + 20;
  int progress_right = l->vol_icon.x - 20;
  if (progress_right < progress_left + 50) {
    progress_right = progress_left + 50;
  }

  l->progress_bg.x = progress_left;
  l->progress_bg.w = progress_right - progress_left;
  l->progress_bg.h = 8;
  l->progress_bg.y = center_y - l->progress_bg.h / 2;
}

void ui_draw_video(const UiContext *ui, VideoState *vid) {
  int tw, th;
  SDL_Texture *tex = video_get_texture(vid, &tw, &th);
  if (!tex) return;

  int ww, wh;
  SDL_GetRendererOutputSize(ui->ren, &ww, &wh);

  double sx = (double)ww / tw;
  double sy = (double)wh / th;
  double sc = sx < sy ? sx : sy;
  int rw = (int)(tw * sc);
  int rh = (int)(th * sc);

  SDL_Rect dst = {(ww - rw) / 2, (wh - rh) / 2, rw, rh};
  SDL_RenderCopy(ui->ren, tex, NULL, &dst);
}

void ui_draw_player_controls(const UiContext *ui, const UiPlayerLayout *l,
                             const VideoState *vid, int paused, int muted) {
  SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);

  const UiPalette *p = ui->pal;
  SDL_Color c;

  c = p->panel;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 230);
  SDL_RenderFillRect(ui->ren, &l->bar);

  int64_t dur = video_get_duration_ms(vid);
  int64_t pos = video_get_position_ms(vid);
  double r = 0.0;
  if (dur > 0) {
    r = (double)pos / (double)dur;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
  }

  SDL_Rect bg = l->progress_bg;
  SDL_Rect fg = bg;
  fg.w = (int)(bg.w * r);

  c = p->panel_header_border;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 220);
  SDL_RenderFillRect(ui->ren, &bg);

  c = p->row_selected_accent;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 255);
  SDL_RenderFillRect(ui->ren, &fg);

  c = p->text_primary;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderDrawRect(ui->ren, &l->btn_play);

  if (paused) {
    SDL_Point p1 = {l->btn_play.x + l->btn_play.w / 3, l->btn_play.y + 6};
    SDL_Point p2 = {l->btn_play.x + l->btn_play.w / 3,
                    l->btn_play.y + l->btn_play.h - 6};
    SDL_Point p3 = {l->btn_play.x + l->btn_play.w - l->btn_play.w / 4,
                    l->btn_play.y + l->btn_play.h / 2};

    SDL_RenderDrawLine(ui->ren, p1.x, p1.y, p2.x, p2.y);
    SDL_RenderDrawLine(ui->ren, p2.x, p2.y, p3.x, p3.y);
    SDL_RenderDrawLine(ui->ren, p3.x, p3.y, p1.x, p1.y);
  } else {
    int pad = 6;
    SDL_Rect bar1 = {l->btn_play.x + pad, l->btn_play.y + pad, 6,
                     l->btn_play.h - 2 * pad};
    SDL_Rect bar2 = {l->btn_play.x + l->btn_play.w - pad - 6,
                     l->btn_play.y + pad, 6, l->btn_play.h - 2 * pad};
    SDL_RenderFillRect(ui->ren, &bar1);
    SDL_RenderFillRect(ui->ren, &bar2);
  }

  SDL_Point pv1 = {l->btn_prev.x + l->btn_prev.w - 4, l->btn_prev.y + 4};
  SDL_Point pv2 = {l->btn_prev.x + 4, l->btn_prev.y + l->btn_prev.h / 2};
  SDL_Point pv3 = {l->btn_prev.x + l->btn_prev.w - 4,
                   l->btn_prev.y + l->btn_prev.h - 4};
  SDL_RenderDrawLine(ui->ren, pv1.x, pv1.y, pv2.x, pv2.y);
  SDL_RenderDrawLine(ui->ren, pv2.x, pv2.y, pv3.x, pv3.y);

  SDL_Point nx1 = {l->btn_next.x + 4, l->btn_next.y + 4};
  SDL_Point nx2 = {l->btn_next.x + l->btn_next.w - 4,
                   l->btn_next.y + l->btn_next.h / 2};
  SDL_Point nx3 = {l->btn_next.x + 4, l->btn_next.y + l->btn_next.h - 4};
  SDL_RenderDrawLine(ui->ren, nx1.x, nx1.y, nx2.x, nx2.y);
  SDL_RenderDrawLine(ui->ren, nx2.x, nx2.y, nx3.x, nx3.y);

  double vol = video_get_volume(vid);
  if (vol < 0.0) vol = 0.0;
  if (vol > 1.0) vol = 1.0;

  c = p->scrollbar_bg;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 220);
  SDL_RenderFillRect(ui->ren, &l->vol_bar);

  SDL_Rect vol_fg = l->vol_bar;
  vol_fg.w = (int)(l->vol_bar.w * vol);
  c = p->scrollbar_handle;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 255);
  SDL_RenderFillRect(ui->ren, &vol_fg);

  c = p->text_primary;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);

  int icon_x = l->vol_icon.x;
  int icon_y = l->vol_icon.y;
  int icon_w = l->vol_icon.w;
  int icon_h = l->vol_icon.h;

  SDL_Color vc = p->text_primary;
  SDL_SetRenderDrawColor(ui->ren, vc.r, vc.g, vc.b, vc.a);

  int body_w = icon_w / 2.5;
  SDL_Rect body = {icon_x, icon_y + icon_h / 4, body_w, icon_h / 2};
  SDL_RenderFillRect(ui->ren, &body);

  int tip_x = body.x + body.w - 10;
  int tip_y = icon_y + icon_h / 2;
  int base_x = icon_x + icon_w - 3;
  int top_y = icon_y + icon_h / 4;
  int mid_y = tip_y;
  int bot_y = icon_y + (icon_h * 3) / 4;

  for (int y = top_y; y <= mid_y; ++y) {
    float s =
        (top_y != mid_y) ? (float)(y - mid_y) / (float)(top_y - mid_y) : 0.0f;

    int xl = tip_x + (int)((base_x - tip_x) * s + 0.5f);
    int xr = base_x;
    if (xl > xr) {
      int tmp = xl;
      xl = xr;
      xr = tmp;
    }
    SDL_RenderDrawLine(ui->ren, xl, y, xr, y);
  }

  for (int y = mid_y; y <= bot_y; ++y) {
    float s =
        (bot_y != mid_y) ? (float)(y - mid_y) / (float)(bot_y - mid_y) : 0.0f;

    int xl = tip_x + (int)((base_x - tip_x) * s + 0.5f);
    int xr = base_x;
    if (xl > xr) {
      int tmp = xl;
      xl = xr;
      xr = tmp;
    }
    SDL_RenderDrawLine(ui->ren, xl, y, xr, y);
  }

  if (muted || vol <= 0.001) {
    SDL_RenderDrawLine(ui->ren, icon_x + 2, icon_y + 2, icon_x + icon_w - 2,
                       icon_y + icon_h - 2);
    SDL_RenderDrawLine(ui->ren, icon_x + icon_w - 2, icon_y + 2, icon_x + 2,
                       icon_y + icon_h - 2);
  }

  if (ui->font_small && dur > 0) {
    char buf[64];
    char cur_str[16], dur_str[16];

    format_time_ms(pos, cur_str, sizeof(cur_str));
    format_time_ms(dur, dur_str, sizeof(dur_str));

    snprintf(buf, sizeof(buf), "%s / %s", cur_str, dur_str);

    SDL_Color col = p->text_primary;
    SDL_Surface *s = TTF_RenderUTF8_Blended(ui->font_small, buf, col);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ui->ren, s);
      SDL_Rect r = {l->progress_bg.x, l->bar.y + 6, s->w, s->h};
      SDL_RenderCopy(ui->ren, t, NULL, &r);
      SDL_DestroyTexture(t);
      SDL_FreeSurface(s);
    }
  }
}

void ui_draw_browser(const UiContext *ui, const FileBrowser *b) {
  if (!b) return;

  int ww, wh;
  SDL_GetRendererOutputSize(ui->ren, &ww, &wh);

  SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);

  const UiPalette *p = ui->pal;
  SDL_Color c;

  c = p->bg;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderClear(ui->ren);

  SDL_Rect panel = {BROWSER_MARGIN, BROWSER_MARGIN, ww - 2 * BROWSER_MARGIN,
                    wh - 2 * BROWSER_MARGIN};

  SDL_Rect shadow = panel;
  shadow.x += 4;
  shadow.y += 4;
  c = p->panel_shadow;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(ui->ren, &shadow);

  c = p->panel;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(ui->ren, &panel);

  SDL_Rect header = panel;
  header.h = BROWSER_HEADER_H;
  c = p->panel_header;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(ui->ren, &header);

  c = p->panel_header_border;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderDrawLine(ui->ren, header.x, header.y + header.h,
                     header.x + header.w, header.y + header.h);

  const int inner_margin = 16;

  if (ui->font_regular) {
    char title[256];

    const char *cwd = b->cwd;
    size_t cwd_len = strlen(cwd);

    if (cwd_len > 200) {
      const char *tail = cwd + (cwd_len - 200);
      snprintf(title, sizeof(title), "Open video ( ...%s )", tail);
    } else {
      snprintf(title, sizeof(title), "Open video ( %s )", cwd);
    }

    SDL_Color col = p->text_primary;
    SDL_Surface *s = TTF_RenderUTF8_Blended(ui->font_regular, title, col);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ui->ren, s);
      SDL_Rect r = {header.x + inner_margin, header.y + (header.h - s->h) / 2,
                    s->w, s->h};
      SDL_RenderCopy(ui->ren, t, NULL, &r);
      SDL_DestroyTexture(t);
      SDL_FreeSurface(s);
    }
  }

  int top = BROWSER_MARGIN * 2 + BROWSER_HEADER_H;
  int max_rows = (wh - top - BROWSER_MARGIN) / BROWSER_LINE_H;
  if (max_rows < 1) max_rows = 1;

  SDL_Rect list_bg = {panel.x + inner_margin, top, panel.w - 2 * inner_margin,
                      wh - top - BROWSER_MARGIN};

  c = p->list_bg;
  SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(ui->ren, &list_bg);

  int total = b->count;
  int visible = max_rows;
  if (visible > total) visible = total;

  int row_area_h = visible * BROWSER_LINE_H;

  for (int i = 0; i < max_rows; ++i) {
    int idx = b->scroll + i;
    if (idx >= b->count) break;

    const BrowserEntry *ent = &b->items[idx];

    SDL_Rect row = {list_bg.x, top + i * BROWSER_LINE_H, list_bg.w,
                    BROWSER_LINE_H - 2};

    if (idx == b->selected) {
      c = p->row_selected_bg;
      SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(ui->ren, &row);

      SDL_Rect accent = row;
      accent.w = 3;
      c = p->row_selected_accent;
      SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(ui->ren, &accent);
    } else {
      c = (i % 2 == 0) ? p->row_even : p->row_odd;
      SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(ui->ren, &row);
    }

    if (ui->font_regular) {
      char buf[256];
      if (ent->is_dir) {
        snprintf(buf, sizeof(buf), "[DIR]  %s", ent->name);
      } else {
        snprintf(buf, sizeof(buf), "      %s", ent->name);
      }

      SDL_Color col = ent->is_dir ? p->text_dir : p->text_primary;
      SDL_Surface *s = TTF_RenderUTF8_Blended(ui->font_regular, buf, col);
      if (s) {
        SDL_Texture *t = SDL_CreateTextureFromSurface(ui->ren, s);
        SDL_Rect r = {row.x + 12, row.y + (row.h - s->h) / 2, s->w, s->h};
        SDL_RenderCopy(ui->ren, t, NULL, &r);
        SDL_DestroyTexture(t);
        SDL_FreeSurface(s);
      }
    }
  }

  if (total > visible && visible > 0) {
    SDL_Rect bar = {list_bg.x + list_bg.w - 6, list_bg.y, 6, row_area_h};

    c = p->scrollbar_bg;
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ui->ren, &bar);

    double frac = (double)visible / (double)total;
    if (frac < 0.1) frac = 0.1;
    int handle_h = (int)(bar.h * frac);

    double pos = 0.0;
    if (total > visible) {
      pos = (double)b->scroll / (double)(total - visible);
    }
    int handle_y = bar.y + (int)((bar.h - handle_h) * pos);

    SDL_Rect handle = {bar.x, handle_y, bar.w, handle_h};

    c = p->scrollbar_handle;
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ui->ren, &handle);
  }

  if (ui->font_small) {
    const char *hint = "↑/↓ Select   Enter Open   Esc Back";
    SDL_Color col = p->text_muted;

    SDL_Surface *s = TTF_RenderUTF8_Blended(ui->font_small, hint, col);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ui->ren, s);
      SDL_Rect r = {panel.x + inner_margin,
                    panel.y + panel.h - s->h - inner_margin / 2, s->w, s->h};
      SDL_RenderCopy(ui->ren, t, NULL, &r);
      SDL_DestroyTexture(t);
      SDL_FreeSurface(s);
    }
  }

  SDL_RenderPresent(ui->ren);
}

int ui_hit_test_rect(const SDL_Rect *r, int mx, int my) {
  return (mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h);
}

int ui_progress_hit_test(const UiPlayerLayout *l, int mx, int my,
                         double *ratio) {
  SDL_Rect bg = l->progress_bg;
  if (mx < bg.x || mx >= bg.x + bg.w || my < bg.y - 4 || my >= bg.y + bg.h + 4)
    return 0;

  double r = (double)(mx - bg.x) / (double)bg.w;
  if (r < 0.0) r = 0.0;
  if (r > 1.0) r = 1.0;
  if (ratio) *ratio = r;
  return 1;
}

int ui_volume_bar_hit_test(const UiPlayerLayout *l, int mx, int my,
                           double *ratio) {
  SDL_Rect bar = l->vol_bar;
  if (!ui_hit_test_rect(&bar, mx, my)) return 0;

  double r = (double)(mx - bar.x) / (double)bar.w;
  if (r < 0.0) r = 0.0;
  if (r > 1.0) r = 1.0;
  if (ratio) *ratio = r;
  return 1;
}
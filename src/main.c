#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <libavformat/avformat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browser.h"
#include "playlist.h"
#include "ui.h"
#include "video.h"

typedef enum { STATE_BROWSE = 0, STATE_PLAY } AppState;

typedef struct {
  SDL_Window *win;
  SDL_Renderer *ren;

  AppState state;

  FileBrowser *browser;

  Playlist pl;
  VideoState vid;
  int paused;
  int fullscreen;

  int muted;
  double volume_before_mute;

  UiContext ui;
} App;

static void player_open_current(App *app) {
  const char *path = playlist_current(&app->pl);
  if (!path) return;

  video_close(&app->vid);
  if (!video_open(&app->vid, app->ren, path)) {
    fprintf(stderr, "Failed to open video: %s\n", path);
    return;
  }

  app->paused = 0;
  SDL_SetWindowTitle(app->win, path);
}

static void app_enter_browse(App *app) {
  video_close(&app->vid);
  playlist_free(&app->pl);

  if (!app->browser) {
    app->browser = browser_create(app->ren, NULL);
  }
  app->state = STATE_BROWSE;
  SDL_SetWindowTitle(app->win, "Choose file / folder");
}

static void app_enter_play(App *app, const char *path) {
  playlist_free(&app->pl);
  if (!playlist_build(&app->pl, path)) {
    return;
  }
  player_open_current(app);
  app->state = STATE_PLAY;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
  av_register_all();
#endif
  avformat_network_init();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() != 0) {
    fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  App app;
  memset(&app, 0, sizeof(app));

  app.win = SDL_CreateWindow("Dummy Player", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1280, 720,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!app.win) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  app.ren = SDL_CreateRenderer(
      app.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!app.ren) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(app.win);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  if (!ui_init(&app.ui, app.ren, "fonts/DejaVuSans.ttf")) {
    fprintf(stderr, "ui_init failed\n");
    SDL_DestroyRenderer(app.ren);
    SDL_DestroyWindow(app.win);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  app.state = STATE_BROWSE;
  app_enter_browse(&app);

  int running = 1;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (app.state == STATE_BROWSE) {
        BrowserResult r = browser_handle_event(app.browser, &e);
        if (r == BROWSER_RESULT_QUIT) {
          running = 0;
        } else if (r == BROWSER_RESULT_PICKED) {
          char *path = browser_take_selected_path(app.browser);
          if (path) {
            app_enter_play(&app, path);
            free(path);
          }
        }
      }

      else if (app.state == STATE_PLAY) {
        if (e.type == SDL_QUIT) {
          running = 0;
        } else if (e.type == SDL_KEYDOWN) {
          SDL_Keycode k = e.key.keysym.sym;

          if (k == SDLK_ESCAPE) {
            running = 0;
          } else if (k == SDLK_SPACE) {
            app.paused = !app.paused;
          } else if (k == SDLK_f) {
            app.fullscreen = !app.fullscreen;
            SDL_SetWindowFullscreen(
                app.win, app.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
          } else if (k == SDLK_RIGHT) {
            int64_t p = video_get_position_ms(&app.vid) + 5000;
            video_seek_ms(&app.vid, p);
          } else if (k == SDLK_LEFT) {
            int64_t p = video_get_position_ms(&app.vid) - 5000;
            video_seek_ms(&app.vid, p);
          } else if (k == SDLK_UP) {
            double v = video_get_volume(&app.vid) + 0.1;
            video_set_volume(&app.vid, v);
            app.muted = (video_get_volume(&app.vid) <= 0.001);
            if (!app.muted) app.volume_before_mute = video_get_volume(&app.vid);
          } else if (k == SDLK_DOWN) {
            double v = video_get_volume(&app.vid) - 0.1;
            video_set_volume(&app.vid, v);
            app.muted = (video_get_volume(&app.vid) <= 0.001);
            if (!app.muted) app.volume_before_mute = video_get_volume(&app.vid);
          } else if (k == SDLK_d) {
            if (playlist_next(&app.pl)) player_open_current(&app);
          } else if (k == SDLK_a) {
            if (playlist_prev(&app.pl)) player_open_current(&app);
          } else if (k == SDLK_o) {
            app_enter_browse(&app);
          }
        } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                   e.button.button == SDL_BUTTON_LEFT) {
          int mx = e.button.x;
          int my = e.button.y;

          UiPlayerLayout lay;
          ui_compute_player_layout(&app.ui, &lay);

          double r;

          if (ui_hit_test_rect(&lay.btn_play, mx, my)) {
            app.paused = !app.paused;
          } else if (ui_hit_test_rect(&lay.btn_prev, mx, my)) {
            if (playlist_prev(&app.pl)) player_open_current(&app);
          } else if (ui_hit_test_rect(&lay.btn_next, mx, my)) {
            if (playlist_next(&app.pl)) player_open_current(&app);
          } else if (ui_hit_test_rect(&lay.vol_icon, mx, my)) {
            if (!app.muted) {
              app.volume_before_mute = video_get_volume(&app.vid);
              video_set_volume(&app.vid, 0.0);
              app.muted = 1;
            } else {
              double v = app.volume_before_mute;
              if (v <= 0.0) v = 1.0;
              video_set_volume(&app.vid, v);
              app.muted = 0;
            }
          } else if (ui_volume_bar_hit_test(&lay, mx, my, &r)) {
            video_set_volume(&app.vid, r);
            app.muted = (r <= 0.001);
            if (!app.muted) app.volume_before_mute = r;
          } else if (ui_progress_hit_test(&lay, mx, my, &r)) {
            int64_t dur = video_get_duration_ms(&app.vid);
            if (dur > 0) {
              int64_t target = (int64_t)(dur * r);
              video_seek_ms(&app.vid, target);
            }
          }
        }
      }
    }

    if (app.state == STATE_BROWSE) {
      ui_draw_browser(&app.ui, app.browser);
    } else if (app.state == STATE_PLAY) {
      if (!app.paused) {
        video_step(&app.vid, app.ren);
        if (video_is_eof(&app.vid)) {
          if (playlist_next(&app.pl)) player_open_current(&app);
        }
      }

      const UiPalette *p = app.ui.pal;
      SDL_Color bg = p->bg;
      SDL_SetRenderDrawColor(app.ren, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderClear(app.ren);

      ui_draw_video(&app.ui, &app.vid);

      UiPlayerLayout lay;
      ui_compute_player_layout(&app.ui, &lay);
      ui_draw_player_controls(&app.ui, &lay, &app.vid, app.paused, app.muted);

      SDL_RenderPresent(app.ren);
    }
  }

  ui_shutdown(&app.ui);
  browser_destroy(app.browser);
  video_close(&app.vid);
  playlist_free(&app.pl);
  SDL_DestroyRenderer(app.ren);
  SDL_DestroyWindow(app.win);
  TTF_Quit();
  SDL_Quit();

  return 0;
}
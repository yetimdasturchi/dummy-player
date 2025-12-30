#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "browser.h"

static void clear_items(FileBrowser *b) {
  if (!b->items) return;
  for (int i = 0; i < b->count; ++i) {
    free(b->items[i].name);
    free(b->items[i].path);
  }
  free(b->items);
  b->items = NULL;
  b->count = 0;
  b->selected = 0;
  b->scroll = 0;
}

static int cmp_entries(const void *a, const void *b) {
  const BrowserEntry *ea = (const BrowserEntry *)a;
  const BrowserEntry *eb = (const BrowserEntry *)b;
  if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
  return strcmp(ea->name, eb->name);
}

static void scan_dir(FileBrowser *b) {
  clear_items(b);

  DIR *d = opendir(b->cwd);
  if (!d) {
    fprintf(stderr, "browser: failed to open dir %s\n", b->cwd);
    return;
  }

  BrowserEntry *arr = NULL;
  int n = 0;
  int cap = 0;

  {
    BrowserEntry up = {0};
    up.is_dir = 1;
    up.name = str_dupe("..");
    up.path = str_dupe("..");

    arr = (BrowserEntry *)malloc(sizeof(BrowserEntry));
    if (!arr || !up.name || !up.path) {
      if (up.name) free(up.name);
      if (up.path) free(up.path);
      if (arr) free(arr);
      closedir(d);
      return;
    }
    arr[0] = up;
    n = 1;
    cap = 1;
  }

  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') continue;

    char full[PATH_MAX];
    size_t cwd_len = strlen(b->cwd);
    size_t name_len = strlen(ent->d_name);

    if (cwd_len + 1 + name_len >= sizeof(full)) {
      continue;
    }

    snprintf(full, sizeof(full), "%s/%s", b->cwd, ent->d_name);

    int is_dir = is_dir_path(full);
    int use = is_dir || is_video_file(full);
    if (!use) continue;

    if (n == cap) {
      int nc = cap ? cap * 2 : 32;
      BrowserEntry *tmp =
          (BrowserEntry *)realloc(arr, (size_t)nc * sizeof(BrowserEntry));
      if (!tmp) break;
      arr = tmp;
      cap = nc;
    }

    arr[n].name = str_dupe(ent->d_name);
    arr[n].path = str_dupe(full);
    arr[n].is_dir = is_dir;

    if (!arr[n].name || !arr[n].path) {
      break;
    }
    n++;
  }
  closedir(d);

  if (n == 0) {
    free(arr);
    return;
  }

  qsort(arr, (size_t)n, sizeof(BrowserEntry), cmp_entries);

  b->items = arr;
  b->count = n;
  b->selected = 0;
  b->scroll = 0;
}

static int browser_visible_rows(FileBrowser *b) {
  int ww, wh;
  SDL_GetRendererOutputSize(b->ren, &ww, &wh);

  int top = BROWSER_MARGIN * 2 + BROWSER_HEADER_H;
  int max_rows = (wh - top - BROWSER_MARGIN) / BROWSER_LINE_H;
  if (max_rows < 1) max_rows = 1;
  return max_rows;
}

FileBrowser *browser_create(SDL_Renderer *ren, const char *start_dir) {
  FileBrowser *b = (FileBrowser *)calloc(1, sizeof(FileBrowser));
  if (!b) return NULL;

  b->ren = ren;

  if (start_dir && start_dir[0]) {
    strncpy(b->cwd, start_dir, sizeof(b->cwd) - 1);
    b->cwd[sizeof(b->cwd) - 1] = '\0';
  } else {
    if (!getcwd(b->cwd, sizeof(b->cwd))) {
      strcpy(b->cwd, ".");
    }
  }

  scan_dir(b);
  return b;
}

void browser_destroy(FileBrowser *b) {
  if (!b) return;
  clear_items(b);
  free(b->picked_path);
  free(b);
}

char *browser_take_selected_path(FileBrowser *b) {
  char *p = b->picked_path;
  b->picked_path = NULL;
  b->result = BROWSER_RESULT_NONE;
  return p;
}

static void navigate_into(FileBrowser *b, BrowserEntry *e) {
  if (!e->is_dir) return;

  if (strcmp(e->name, "..") == 0) {
    char *slash = strrchr(b->cwd, '/');
#ifdef _WIN32
    char *bslash = strrchr(b->cwd, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (slash && slash != b->cwd) {
      *slash = '\0';
    } else {
      strcpy(b->cwd, ".");
    }
  } else {
    strncpy(b->cwd, e->path, sizeof(b->cwd) - 1);
    b->cwd[sizeof(b->cwd) - 1] = '\0';
  }

  scan_dir(b);
}

BrowserResult browser_handle_event(FileBrowser *b, const SDL_Event *e) {
  if (!b) return BROWSER_RESULT_NONE;
  if (b->result != BROWSER_RESULT_NONE) return b->result;

  switch (e->type) {
    case SDL_QUIT:
      b->result = BROWSER_RESULT_QUIT;
      break;

    case SDL_KEYDOWN: {
      SDL_Keycode k = e->key.keysym.sym;
      if (k == SDLK_ESCAPE) {
        b->result = BROWSER_RESULT_QUIT;
      } else if (k == SDLK_DOWN) {
        if (b->count > 0) {
          b->selected++;
          if (b->selected >= b->count) b->selected = b->count - 1;
          int visible_rows = browser_visible_rows(b);
          if (b->selected >= b->scroll + visible_rows)
            b->scroll = b->selected - visible_rows + 1;
        }
      } else if (k == SDLK_UP) {
        if (b->count > 0) {
          b->selected--;
          if (b->selected < 0) b->selected = 0;
          if (b->selected < b->scroll) b->scroll = b->selected;
        }
      } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (b->selected >= 0 && b->selected < b->count) {
          BrowserEntry *ent = &b->items[b->selected];
          if (ent->is_dir) {
            navigate_into(b, ent);
          } else {
            free(b->picked_path);
            b->picked_path = str_dupe(ent->path);
            b->result = BROWSER_RESULT_PICKED;
          }
        }
      }
      break;
    }

    case SDL_MOUSEWHEEL: {
      int visible_rows = browser_visible_rows(b);
      b->scroll -= e->wheel.y;
      if (b->scroll < 0) b->scroll = 0;
      int max_scroll = b->count - visible_rows;
      if (max_scroll < 0) max_scroll = 0;
      if (b->scroll > max_scroll) b->scroll = max_scroll;
      break;
    }

    case SDL_MOUSEBUTTONDOWN:
      if (e->button.button == SDL_BUTTON_LEFT) {
        int my = e->button.y;

        int top = BROWSER_MARGIN * 2 + BROWSER_HEADER_H;
        if (my >= top) {
          int idx = (my - top) / BROWSER_LINE_H + b->scroll;
          if (idx >= 0 && idx < b->count) {
            b->selected = idx;
            BrowserEntry *ent = &b->items[idx];
            if (ent->is_dir) {
              navigate_into(b, ent);
            } else {
              free(b->picked_path);
              b->picked_path = str_dupe(ent->path);
              b->result = BROWSER_RESULT_PICKED;
            }
          }
        }
      }
      break;

    default:
      break;
  }

  return b->result;
}
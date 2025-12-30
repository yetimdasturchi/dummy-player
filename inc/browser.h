#pragma once

#include <SDL2/SDL.h>
#include <limits.h>

#include "common.h"

#define BROWSER_MARGIN 24
#define BROWSER_LINE_H 28
#define BROWSER_HEADER_H 40

typedef struct {
  char *name;
  char *path;
  int is_dir;
} BrowserEntry;

typedef enum {
  BROWSER_RESULT_NONE = 0,
  BROWSER_RESULT_PICKED,
  BROWSER_RESULT_QUIT
} BrowserResult;

typedef struct FileBrowser {
  SDL_Renderer *ren;
  char cwd[PATH_MAX];

  BrowserEntry *items;
  int count;
  int selected;
  int scroll;

  BrowserResult result;
  char *picked_path;
} FileBrowser;

FileBrowser *browser_create(SDL_Renderer *ren, const char *start_dir);
void browser_destroy(FileBrowser *b);

BrowserResult browser_handle_event(FileBrowser *b, const SDL_Event *e);
char *browser_take_selected_path(FileBrowser *b);
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "playlist.h"

void playlist_free(Playlist *pl) {
  if (!pl) return;
  if (pl->files) {
    for (int i = 0; i < pl->count; ++i) {
      free(pl->files[i]);
    }
    free(pl->files);
  }
  memset(pl, 0, sizeof(*pl));
}

static int collect_from_dir(Playlist *pl, const char *dir,
                            const char *selected) {
  DIR *d = opendir(dir);
  if (!d) {
    fprintf(stderr, "playlist: failed to open dir %s\n", dir);
    return 0;
  }

  char **arr = NULL;
  int n = 0, cap = 0;

  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') continue;

    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
    if (!is_video_file(full)) continue;

    if (n == cap) {
      int nc = cap ? cap * 2 : 32;
      char **tmp = (char **)realloc(arr, (size_t)nc * sizeof(char *));
      if (!tmp) break;
      arr = tmp;
      cap = nc;
    }
    arr[n++] = str_dupe(full);
  }
  closedir(d);

  if (n == 0) {
    free(arr);
    fprintf(stderr, "playlist: no video files in %s\n", dir);
    return 0;
  }

  qsort(arr, (size_t)n, sizeof(char *), cmp_str);

  pl->files = arr;
  pl->count = n;
  pl->index = 0;

  if (selected) {
    for (int i = 0; i < n; ++i) {
      if (strcmp(arr[i], selected) == 0) {
        pl->index = i;
        break;
      }
    }
  }

  return 1;
}

int playlist_build(Playlist *pl, const char *path) {
  playlist_free(pl);
  if (!path || !path[0]) return 0;

  if (is_dir_path(path)) {
    return collect_from_dir(pl, path, NULL);
  }

  if (!is_video_file(path)) {
    fprintf(stderr, "playlist: not a video file: %s\n", path);
    return 0;
  }

  char dir[PATH_MAX];
  char name[PATH_MAX];
  strncpy(name, path, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';

  char *slash = strrchr(name, '/');
#ifdef _WIN32
  char *bslash = strrchr(name, '\\');
  if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
  if (slash) {
    *slash = '\0';
    strncpy(dir, name, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
  } else {
    strcpy(dir, ".");
  }

  return collect_from_dir(pl, dir, path);
}

const char *playlist_current(const Playlist *pl) {
  if (!pl || pl->count == 0) return NULL;
  if (pl->index < 0 || pl->index >= pl->count) return NULL;
  return pl->files[pl->index];
}

int playlist_next(Playlist *pl) {
  if (!pl || pl->count == 0) return 0;
  pl->index = (pl->index + 1) % pl->count;
  return 1;
}

int playlist_prev(Playlist *pl) {
  if (!pl || pl->count == 0) return 0;
  pl->index = (pl->index - 1 + pl->count) % pl->count;
  return 1;
}
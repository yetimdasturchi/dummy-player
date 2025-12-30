#pragma once

typedef struct {
  char **files;
  int count;
  int index;
} Playlist;

int playlist_build(Playlist *pl, const char *path);
void playlist_free(Playlist *pl);

const char *playlist_current(const Playlist *pl);
int playlist_next(Playlist *pl);
int playlist_prev(Playlist *pl);
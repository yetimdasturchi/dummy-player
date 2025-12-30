#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

int ends_with_ci(const char *s, const char *ext) {
  size_t ls = strlen(s), le = strlen(ext);
  if (le > ls) return 0;
  const char *a = s + (ls - le);
  for (size_t i = 0; i < le; ++i) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)ext[i]))
      return 0;
  }
  return 1;
}

int is_video_file(const char *p) {
  return ends_with_ci(p, ".mp4") || ends_with_ci(p, ".mkv") ||
         ends_with_ci(p, ".avi") || ends_with_ci(p, ".mov");
}

int is_dir_path(const char *p) {
  struct stat st;
  if (stat(p, &st) != 0) return 0;
  return S_ISDIR(st.st_mode);
}

char *str_dupe(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}

int cmp_str(const void *a, const void *b) {
  const char *sa = *(const char *const *)a;
  const char *sb = *(const char *const *)b;
  return strcmp(sa, sb);
}

void format_time_ms(int64_t ms, char *buf, size_t buf_size) {
  if (!buf || buf_size == 0) {
    return;
  }

  if (ms < 0) ms = 0;
  int64_t total_sec = ms / 1000;
  int sec = (int)(total_sec % 60);
  int min = (int)((total_sec / 60) % 60);
  int hour = (int)(total_sec / 3600);

  if (hour > 99) hour = 99;

  if (hour > 0) {
    (void)snprintf(buf, buf_size, "%02d:%02d:%02d", hour, min, sec);
  } else {
    (void)snprintf(buf, buf_size, "%02d:%02d", min, sec);
  }
}
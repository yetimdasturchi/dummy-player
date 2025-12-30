#pragma once

#include <stddef.h>
#include <stdint.h>

int ends_with_ci(const char *s, const char *ext);
int is_video_file(const char *p);
int is_dir_path(const char *p);
char *str_dupe(const char *s);
int cmp_str(const void *a, const void *b);

void format_time_ms(int64_t ms, char *buf, size_t buf_size);
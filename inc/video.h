#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>

struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct SwsContext;
struct SwrContext;
struct AVFrame;
struct AVPacket;

typedef struct VideoState {
  struct AVFormatContext *fmt;
  struct AVCodecContext *vdec;
  struct AVCodecContext *adec;
  struct AVStream *vst;
  struct AVStream *ast;

  struct SwsContext *sws;
  struct SwrContext *swr;

  struct AVFrame *vframe;
  struct AVFrame *yuv;
  uint8_t *yuv_buf;
  int yuv_buf_size;

  struct AVFrame *aframe;
  struct AVPacket *pkt;

  int v_stream_index;
  int a_stream_index;

  SDL_Texture *tex;
  int tex_w, tex_h;

  SDL_AudioDeviceID audio_dev;
  int audio_sample_rate;
  int audio_channels;
  int audio_bytes_per_sample;

  int frame_ms;
  Uint32 last_ticks;

  int64_t duration_ms;
  int64_t cur_pts_ms;

  double volume;
  int eof;
} VideoState;

int video_open(VideoState *v, SDL_Renderer *ren, const char *path);
void video_close(VideoState *v);

void video_step(VideoState *v, SDL_Renderer *ren);

void video_seek_ms(VideoState *v, int64_t target_ms);

void video_set_volume(VideoState *v, double volume);
double video_get_volume(const VideoState *v);

int64_t video_get_duration_ms(const VideoState *v);
int64_t video_get_position_ms(const VideoState *v);

int video_is_eof(const VideoState *v);

SDL_Texture *video_get_texture(VideoState *v, int *w, int *h);
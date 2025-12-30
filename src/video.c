#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "video.h"

static void video_internal_close(VideoState *v) {
  if (!v) return;

  if (v->tex) SDL_DestroyTexture(v->tex);
  if (v->sws) sws_freeContext(v->sws);
  if (v->swr) swr_free(&v->swr);
  if (v->vdec) avcodec_free_context(&v->vdec);
  if (v->adec) avcodec_free_context(&v->adec);
  if (v->fmt) avformat_close_input(&v->fmt);
  if (v->vframe) av_frame_free(&v->vframe);
  if (v->yuv) av_frame_free(&v->yuv);
  if (v->yuv_buf) av_free(v->yuv_buf);
  if (v->aframe) av_frame_free(&v->aframe);
  if (v->pkt) av_packet_free(&v->pkt);
  if (v->audio_dev) SDL_CloseAudioDevice(v->audio_dev);

  memset(v, 0, sizeof(*v));
}

void video_close(VideoState *v) { video_internal_close(v); }

int video_open(VideoState *v, SDL_Renderer *ren, const char *path) {
  video_internal_close(v);
  memset(v, 0, sizeof(*v));
  v->v_stream_index = -1;
  v->a_stream_index = -1;
  v->volume = 1.0;

  if (avformat_open_input(&v->fmt, path, NULL, NULL) < 0) {
    fprintf(stderr, "video: cannot open '%s'\n", path);
    return 0;
  }
  if (avformat_find_stream_info(v->fmt, NULL) < 0) {
    fprintf(stderr, "video: cannot find stream info\n");
    video_internal_close(v);
    return 0;
  }

  int si = av_find_best_stream(v->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (si < 0) {
    fprintf(stderr, "video: no video stream\n");
    video_internal_close(v);
    return 0;
  }
  v->vst = v->fmt->streams[si];
  v->v_stream_index = si;

  {
    AVCodecParameters *par = v->vst->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
      fprintf(stderr, "video: decoder not found\n");
      video_internal_close(v);
      return 0;
    }
    v->vdec = avcodec_alloc_context3(codec);
    if (!v->vdec || avcodec_parameters_to_context(v->vdec, par) < 0 ||
        avcodec_open2(v->vdec, codec, NULL) < 0) {
      fprintf(stderr, "video: failed to open decoder\n");
      video_internal_close(v);
      return 0;
    }
  }

  v->vframe = av_frame_alloc();
  v->pkt = av_packet_alloc();
  if (!v->vframe || !v->pkt) {
    video_internal_close(v);
    return 0;
  }

  v->tex_w = v->vdec->width;
  v->tex_h = v->vdec->height;
  if (v->tex_w <= 0 || v->tex_h <= 0) {
    v->tex_w = 640;
    v->tex_h = 360;
  }

  v->tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_YV12,
                             SDL_TEXTUREACCESS_STREAMING, v->tex_w, v->tex_h);
  if (!v->tex) {
    fprintf(stderr, "video: SDL_CreateTexture failed: %s\n", SDL_GetError());
    video_internal_close(v);
    return 0;
  }

  v->sws = sws_getContext(v->vdec->width, v->vdec->height, v->vdec->pix_fmt,
                          v->tex_w, v->tex_h, AV_PIX_FMT_YUV420P, SWS_BILINEAR,
                          NULL, NULL, NULL);
  if (!v->sws) {
    fprintf(stderr, "video: sws_getContext failed\n");
    video_internal_close(v);
    return 0;
  }

  v->yuv = av_frame_alloc();
  if (!v->yuv) {
    video_internal_close(v);
    return 0;
  }
  v->yuv_buf_size =
      av_image_get_buffer_size(AV_PIX_FMT_YUV420P, v->tex_w, v->tex_h, 1);
  v->yuv_buf = (uint8_t *)av_malloc(v->yuv_buf_size);
  if (!v->yuv_buf) {
    video_internal_close(v);
    return 0;
  }
  av_image_fill_arrays(v->yuv->data, v->yuv->linesize, v->yuv_buf,
                       AV_PIX_FMT_YUV420P, v->tex_w, v->tex_h, 1);

  v->duration_ms = 0;
  if (v->fmt->duration > 0 && v->fmt->duration != AV_NOPTS_VALUE) {
    v->duration_ms = v->fmt->duration / (AV_TIME_BASE / 1000);
  }

  AVRational fr = v->vst->avg_frame_rate;
  if (fr.num > 0 && fr.den > 0) {
    double fps = (double)fr.num / (double)fr.den;
    if (fps <= 0 || fps > 120.0) fps = 25.0;
    v->frame_ms = (int)(1000.0 / fps);
  } else {
    v->frame_ms = 40;
  }

  v->a_stream_index = -1;
  v->audio_dev = 0;
  v->swr = NULL;
  v->aframe = NULL;

  int ai = av_find_best_stream(v->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (ai >= 0) {
    v->ast = v->fmt->streams[ai];
    v->a_stream_index = ai;

    AVCodecParameters *apar = v->ast->codecpar;
    const AVCodec *acodec = avcodec_find_decoder(apar->codec_id);
    if (acodec) {
      v->adec = avcodec_alloc_context3(acodec);
      if (v->adec && avcodec_parameters_to_context(v->adec, apar) >= 0 &&
          avcodec_open2(v->adec, acodec, NULL) >= 0) {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = v->adec->sample_rate > 0 ? v->adec->sample_rate : 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 4096;
        want.callback = NULL;

        v->audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (v->audio_dev) {
          SDL_PauseAudioDevice(v->audio_dev, 0);
          v->audio_sample_rate = have.freq;
          v->audio_channels = have.channels;
          v->audio_bytes_per_sample = SDL_AUDIO_BITSIZE(have.format) / 8;

          int64_t in_ch_layout = v->adec->channel_layout;
          if (!in_ch_layout) {
            in_ch_layout = av_get_default_channel_layout(v->adec->channels);
          }
          int64_t out_ch_layout =
              (have.channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

          v->swr = swr_alloc_set_opts(
              NULL, out_ch_layout, AV_SAMPLE_FMT_S16, have.freq, in_ch_layout,
              v->adec->sample_fmt, v->adec->sample_rate, 0, NULL);
          if (v->swr && swr_init(v->swr) >= 0) {
            v->aframe = av_frame_alloc();
          } else {
            if (v->swr) swr_free(&v->swr);
            SDL_CloseAudioDevice(v->audio_dev);
            v->audio_dev = 0;
            avcodec_free_context(&v->adec);
            v->adec = NULL;
          }
        } else {
          avcodec_free_context(&v->adec);
          v->adec = NULL;
        }
      }
    }
  }

  v->last_ticks = SDL_GetTicks();
  v->cur_pts_ms = 0;
  v->eof = 0;

  return 1;
}

static void video_queue_audio(VideoState *v) {
  if (!v->adec || !v->swr || !v->audio_dev || !v->aframe) return;

  int out_channels = v->audio_channels > 0 ? v->audio_channels : 2;
  int out_rate =
      v->audio_sample_rate > 0 ? v->audio_sample_rate : v->adec->sample_rate;

  int out_samples = (int)av_rescale_rnd(
      swr_get_delay(v->swr, v->adec->sample_rate) + v->aframe->nb_samples,
      out_rate, v->adec->sample_rate, AV_ROUND_UP);

  uint8_t *out_buf = NULL;
  int ret = av_samples_alloc(&out_buf, NULL, out_channels, out_samples,
                             AV_SAMPLE_FMT_S16, 0);
  if (ret < 0) return;

  int conv =
      swr_convert(v->swr, &out_buf, out_samples,
                  (const uint8_t **)v->aframe->data, v->aframe->nb_samples);
  if (conv > 0) {
    int data_size = av_samples_get_buffer_size(NULL, out_channels, conv,
                                               AV_SAMPLE_FMT_S16, 1);
    if (data_size > 0) {
      int16_t *samples = (int16_t *)out_buf;
      int total = data_size / (int)sizeof(int16_t);
      double vol = v->volume;
      for (int i = 0; i < total; ++i) {
        int s = samples[i];
        s = (int)(s * vol);
        if (s < -32768) s = -32768;
        if (s > 32767) s = 32767;
        samples[i] = (int16_t)s;
      }
      SDL_QueueAudio(v->audio_dev, out_buf, (Uint32)data_size);
    }
  }

  av_freep(&out_buf);
}

static int video_decode_next(VideoState *v) {
  for (;;) {
    int ret = av_read_frame(v->fmt, v->pkt);
    if (ret < 0) {
      v->eof = 1;
      return ret;
    }

    if (v->pkt->stream_index == v->v_stream_index) {
      ret = avcodec_send_packet(v->vdec, v->pkt);
      av_packet_unref(v->pkt);
      if (ret < 0) continue;

      while ((ret = avcodec_receive_frame(v->vdec, v->vframe)) >= 0) {
        return 0;
      }
    } else if (v->adec && v->pkt->stream_index == v->a_stream_index) {
      ret = avcodec_send_packet(v->adec, v->pkt);
      av_packet_unref(v->pkt);
      if (ret < 0) continue;

      while ((ret = avcodec_receive_frame(v->adec, v->aframe)) >= 0) {
        video_queue_audio(v);
        av_frame_unref(v->aframe);
      }
    } else {
      av_packet_unref(v->pkt);
    }
  }
}

void video_step(VideoState *v, SDL_Renderer *ren) {
  (void)ren;
  if (!v || !v->fmt || !v->vdec || !v->tex || v->eof) return;

  Uint32 now = SDL_GetTicks();

  int need_decode = 0;

  if ((int)(now - v->last_ticks) >= v->frame_ms) {
    need_decode = 1;
  }

  if (v->audio_dev && v->audio_sample_rate > 0 && v->audio_channels > 0 &&
      v->audio_bytes_per_sample > 0) {
    Uint32 queued = SDL_GetQueuedAudioSize(v->audio_dev);

    Uint32 low_limit = (Uint32)(v->audio_sample_rate * v->audio_channels *
                                v->audio_bytes_per_sample / 4);

    if (queued < low_limit) {
      need_decode = 1;
    }
  }

  if (!need_decode) return;

  if (video_decode_next(v) < 0) return;

  if ((int)(now - v->last_ticks) >= v->frame_ms) {
    sws_scale(v->sws, (const uint8_t *const *)v->vframe->data,
              v->vframe->linesize, 0, v->vdec->height, v->yuv->data,
              v->yuv->linesize);

    SDL_UpdateYUVTexture(v->tex, NULL, v->yuv->data[0], v->yuv->linesize[0],
                         v->yuv->data[1], v->yuv->linesize[1], v->yuv->data[2],
                         v->yuv->linesize[2]);

    if (v->vframe->best_effort_timestamp != AV_NOPTS_VALUE) {
      int64_t pts = v->vframe->best_effort_timestamp;
      AVRational tb = v->vst->time_base;
      int64_t ms = av_rescale_q(pts, tb, (AVRational){1, 1000});
      v->cur_pts_ms = ms;
    }

    v->last_ticks = now;
  }
}

void video_seek_ms(VideoState *v, int64_t target_ms) {
  if (!v || !v->fmt || !v->vst) return;
  if (target_ms < 0) target_ms = 0;
  if (v->duration_ms > 0 && target_ms > v->duration_ms)
    target_ms = v->duration_ms;

  int64_t ts =
      av_rescale_q(target_ms, (AVRational){1, 1000}, v->vst->time_base);

  if (av_seek_frame(v->fmt, v->v_stream_index, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
    avcodec_flush_buffers(v->vdec);
    if (v->adec) avcodec_flush_buffers(v->adec);
    if (v->audio_dev) SDL_ClearQueuedAudio(v->audio_dev);
    v->cur_pts_ms = target_ms;
    v->last_ticks = SDL_GetTicks();
    v->eof = 0;
  }
}

void video_set_volume(VideoState *v, double volume) {
  if (!v) return;
  if (volume < 0.0) volume = 0.0;
  if (volume > 1.0) volume = 1.0;
  v->volume = volume;
}

double video_get_volume(const VideoState *v) { return v ? v->volume : 0.0; }

int64_t video_get_duration_ms(const VideoState *v) {
  return v ? v->duration_ms : 0;
}

int64_t video_get_position_ms(const VideoState *v) {
  return v ? v->cur_pts_ms : 0;
}

int video_is_eof(const VideoState *v) { return v ? v->eof : 0; }

SDL_Texture *video_get_texture(VideoState *v, int *w, int *h) {
  if (!v) return NULL;
  if (w) *w = v->tex_w;
  if (h) *h = v->tex_h;
  return v->tex;
}
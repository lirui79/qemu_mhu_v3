#ifndef __FFMPEG_WRAPPER_H__
#define __FFMPEG_WRAPPER_H__

#ifdef USING_FFMPEG
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavformat/avformat.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"

typedef struct ff_context {
    AVFormatContext *fmt_ctx;
    AVDictionary *format_opts;
    AVBSFContext *bsf_ctx;
    int video_stream_idx;
    int eof;
    int raw_data;
    AVPacket pkt;
} ff_context, *ff_context_ptr;

int ff_open(const char *file_name, struct ff_context *ctx);
void ff_close(struct ff_context *ctx);
unsigned int ff_read_frame(unsigned char *buffer, unsigned int buffer_size, unsigned long *pts,
                           struct ff_context *ctx);
int ff_seek_to_start(struct ff_context *ctx /* , uint64_t pts */);
static inline int ff_eof(const struct ff_context *ctx) { return ctx->eof; }
static inline enum AVCodecID ff_video_codec(const struct ff_context *ctx)
{
    return ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id;
}

static inline int ff_image_alloc(uint8_t *pointers[4], int linesizes[4],
                   int w, int h, enum AVPixelFormat pix_fmt, int align)
{
    return av_image_alloc(pointers, linesizes, w, h, pix_fmt, align);
}

static inline void ff_image_copy(uint8_t *dst_data[4], int dst_linesizes[4],
                   const uint8_t *src_data[4], const int src_linesizes[4],
                   enum AVPixelFormat pix_fmt, int width, int height)
{
    av_image_copy(dst_data, dst_linesizes, src_data, src_linesizes, pix_fmt, width, height);
}

// *****************************************************************************************
// !!! DO NOT mix use ff_open2/ff_close2/ff_read_frame2 with ff_open/ff_close/ff_read_frame
// *****************************************************************************************
ff_context_ptr ff_open2(const char *file_name);
void ff_close2(ff_context_ptr *pctx);
unsigned int ff_read_frame2(ff_context_ptr ctx, unsigned char **pdata, unsigned long *pts);

void ff_set_log_level(int level);
static inline void ff_set_log_callback(void (*callback)(void *, int, const char *, va_list))
{
    av_log_set_callback(callback);
}
#endif  // USING_FFMPEG
#endif
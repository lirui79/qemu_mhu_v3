#ifndef __STREAM_H__
#define __STREAM_H__

#include "defs.h"
#include "vmpp_common.h"
#include <fcntl.h>
#include <stdio.h>

#define READ_FRAME_OPTIMIZE

//#define ADAPTIVE_FRAME_BUFFER

enum { BIT_STREAM_H264, BIT_STREAM_HEVC, BIT_STREAM_AV1, BIT_STREAM_VP9, BIT_STREAM_AVS2, BIT_STREAM_JPEG };
enum RdrFileFormat{ BIT_STREAM_VP9_IVF, BIT_STREAM_AV1_IVF, BIT_STREAM_AV1_AV1, BIT_STREAM_AV1_OBU, BIT_STREAM_AV1_AVIF, BIT_STREAM_UNKNOWN};
#ifdef READ_FRAME_OPTIMIZE
enum { BUFFER_NO_USE, BUFFER_IN_USE, BUFFER_USE_UP};
#endif

typedef struct {
  int type;
  int header_size;
  int total_size;
  int payload_size;
  int has_extension;
  int has_size_field;
  int temporal_layer_id;
  int spatial_layer_id;
} obuHeader_t;

typedef struct {
  unsigned char signature[4];  //='DKIF';
  unsigned short version;      //= 0;
  unsigned short headersize;   //= 32;
  unsigned int FourCC;
  unsigned short width;
  unsigned short height;
  unsigned int rate;
  unsigned int scale;
  unsigned int length;
  unsigned char unused[4];
} IVF_HEADER;

#pragma pack(4)
typedef struct {

  unsigned int frame_size;
  unsigned long long time_stamp;

} IVF_FRAME_HEADER;
#pragma pack()


typedef struct stream_context {
    FILE *file;
    char path[MAX_PATH_LEN];
    off_t size;
    off_t offset;
    int type;
    int eof;
    uint32_t ivf_headers_read;
    IVF_HEADER ivf_header;
    enum RdrFileFormat format;
    void *buffer;
    uint32_t buffer_size;
#ifdef READ_FRAME_OPTIMIZE
    uint32_t buffer_data_len;
    off_t buffer_offset;
    int buffer_type;
    uint32_t buffer_zero_count;
#endif
} stream_context, *stream_context_ptr;

stream_context_ptr stream_open(const char *file_name, int type);
void stream_close(stream_context_ptr *pctx);
int stream_read_frame(stream_context_ptr ctx, uint8_t **pdata);
static inline void stream_seek_to_start(struct stream_context *ctx)
{
    if (ctx && ctx->file)
        fseeko(ctx->file, 0, SEEK_SET);
#ifdef READ_FRAME_OPTIMIZE
    ctx->eof = 0;
    ctx->offset = 0;

    ctx->buffer_data_len = 0;
    ctx->buffer_offset = 0;
    ctx->buffer_zero_count = 0;
    ctx->buffer_type = BUFFER_NO_USE;
#endif
    ctx->ivf_headers_read = 0;
};
static inline int stream_eof(const struct stream_context *ctx) { return ctx->eof; }
static inline off_t stream_offset(const struct stream_context *ctx) { return ctx->offset; }
static inline off_t stream_size(const struct stream_context *ctx) { return ctx->size; }


struct raw_context {
    FILE *file;
    char path[MAX_PATH_LEN];
    vmppPixelFormat format;
    off_t size;
    int pic_size;
    int comp1_size;
    int comp2_size;
    int comp3_size;
    int stride[3];
    int width;
    int height;
    int eof;
};
int raw_open(const char *file_name, vmppPixelFormat fmt, int width, int height, int stride,
             struct raw_context *ctx);
void raw_close(struct raw_context *ctx);
int raw_read_frame(struct raw_context *ctx, vmppFrame *frame);
int raw_pic_size(const struct raw_context *ctx, int *comp1, int *comp2, int *comp3);
static inline void raw_seek_to_start(struct raw_context *ctx)
{
    if (ctx && ctx->file)
        fseeko(ctx->file, 0, SEEK_SET);
};
static inline int raw_eof(const struct raw_context *ctx) { return ctx->eof; }
static inline off_t raw_size(const struct raw_context *ctx) { return ctx->size; }

#endif
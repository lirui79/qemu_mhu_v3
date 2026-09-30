#include "stream.h"
#include "defs.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

#define NAL_SLICE_CRA 21
#define NAL_SLICE_IDR 5
#define NAL_H264_SEI 6
#define NAL_H264_SPS 7
#define NAL_H264_PPS 8
#define NAL_H264_AUD 9
#define NAL_HEVC_VPS 32
#define NAL_HEVC_SPS 33
#define NAL_HEVC_PPS 34
#define NAL_HEVC_AUD 35
#define NAL_HEVC_PREFIX_SEI 39
#define NAL_HEVC_SUFFIX_SEI 40
//AVS2
#define I_PICTURE_START_CODE 0xB3
#define PB_PICTURE_START_CODE 0xB6
#define SLICE_START_CODE_MIN 0x00
#define SLICE_START_CODE_MAX 0x8F
#define VIDEO_SEQ_START_CODE 0xB0

enum { BS_NO_BOUNDARY, BS_BOUNDARY, BS_BOUNDARY_NON_SLICE_NAL };

#ifdef ADAPTIVE_FRAME_BUFFER
    #define DEFAULT_STREAM_BUFFER_SIZE (1024 * 1024)
#else
    #define DEFAULT_STREAM_BUFFER_SIZE (16 * 1024 * 1024)
#endif

    #define MAX_STREAM_BUFFER_SIZE (16 * 1024 * 1024)

#if 0
static int8_t find_next_nal_start(FILE *finput, off_t *start)
{
    int32_t zero_count = 0;
    char byte;
    off_t nal_start = 0;

    while (1) {
        fread(&byte, 1, 1, finput);
        if (feof(finput)) {
            break;
        }

        if (byte == 0) {
            zero_count++;
        } else if (byte == 1 && zero_count >= 2) {
            /* we found the start code!
             * we can have one extra leading zero byte and the rest are
             * considered trailing zeros for the previous unit
             */
            nal_start = ftello(finput);
            nal_start -= 1;                                 /* 1 byte */
            nal_start -= (zero_count > 3 ? 3 : zero_count); /* max 3 zero bytes */

            break;
        } else {
            /* reset zero count and try again*/
            zero_count = 0;
        }
    }

    *start = nal_start;

    if (nal_start == 0 && feof(finput))
        return -1;
    else
        return 0;
}

static uint32_t next_nal_from_file(FILE *finput, uint8_t *stream_buff, uint32_t buff_size)
{
    off_t next_nal_start, nal_start, nal_size;

    int eof;

    /* start of current NAL */
    eof = find_next_nal_start(finput, &nal_start);

    if (eof) {
        return 0;
    }

    /* start of next NAL */
    eof = find_next_nal_start(finput, &next_nal_start);

    if (eof) {
        /* last NAL of the stream */
        fseeko(finput, 0, SEEK_END);
        next_nal_start = ftello(finput);
    }

    fseeko(finput, nal_start, SEEK_SET);

    nal_size = next_nal_start - nal_start;

    if (nal_size > buff_size) {
        LOG_ERROR("NAL does not fit provided buffer");
        return 0;
    }

    fread(stream_buff, 1, nal_size, finput);

    return (uint32_t)nal_size;
}
#endif

static uint32_t get_bytes(uint8_t *stream, uint32_t idx, uint8_t *buffer, uint32_t buffer_length)
{
    uint32_t offset = (size_t)stream - (size_t)buffer;
    if (offset + idx < buffer_length)
        return buffer[offset + idx];
    else
        return buffer[offset + idx - buffer_length];
}

static uint32_t find_image_eoi(uint8_t *stream, uint32_t stream_length, uint32_t *p_offset,
                               uint8_t *buffer, uint32_t buf_len)
{
    uint32_t i, j;
    uint32_t jpeg_thumb_in_stream = 0;
    uint32_t tmp, tmp1, tmp_total = 0;

    *p_offset = 0;
    for (i = 0; i < stream_length; ++i) {
        if (0xFF == get_bytes(stream, i, buffer, buf_len)) {
            /* if 0xFFE1 to 0xFFFD ==> skip  */

            if( (((i + 1) < stream_length) &&
          0xE1 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE2 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE3 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE4 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE5 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE6 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE7 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE8 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE9 == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEA == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEB == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEC == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xED == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEE == get_bytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEF == get_bytes(stream, i + 1, buffer, buf_len)) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                    return (-1);

                /* get length */
                tmp = get_bytes(stream, i, buffer, buf_len);
                tmp1 = get_bytes(stream, i + 1, buffer, buf_len);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                    return (-1);
                /* update */
                i += tmp_total - 1;
                continue;
            }

            /* if 0xFFC2 to 0xFFCB ==> skip  */
            if ((((i + 1) < stream_length) && 0xC1 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC2 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC3 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC5 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC6 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC7 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC8 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xC9 == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xCA == get_bytes(stream, i + 1, buffer, buf_len)) ||
                (((i + 1) < stream_length) && 0xCB == get_bytes(stream, i + 1, buffer, buf_len))) {
                /* increase counter */
                i += 2;

                /* check length vs. data */
                if ((i + 1) > (stream_length))
                    return (-1);

                /* get length */
                tmp = get_bytes(stream, i, buffer, buf_len);
                tmp1 = get_bytes(stream, i + 1, buffer, buf_len);
                tmp_total = (tmp << 8) | tmp1;

                /* check length vs. data */
                if ((tmp_total + i) > (stream_length))
                    return (-1);
                /* update */
                i += tmp_total - 1;

                /* look for EOI */
                for (j = i; j < stream_length; ++j) {
                    if (0xFF == get_bytes(stream, j, buffer, buf_len)) {
                        /* EOI */
                        if (((j + 1) < stream_length) &&
                            0xD9 == get_bytes(stream, j + 1, buffer, buf_len)) {
                            /* check length vs. data */
                            if ((j + 2) >= (stream_length)) {
                                *p_offset = j + 2;
                                return (0);
                            }
                            /* update */
                            i = j;
                            /* stil data left ==> continue */
                            continue;
                        }
                    }
                }
            }

            /* check if thumbnails in stream */
            if (((i + 1) < stream_length) && 0xE0 == get_bytes(stream, i + 1, buffer, buf_len)) {
                if (((i + 9) < stream_length) &&
                    0x4A == get_bytes(stream, i + 4, buffer, buf_len) &&
                    0x46 == get_bytes(stream, i + 5, buffer, buf_len) &&
                    0x58 == get_bytes(stream, i + 6, buffer, buf_len) &&
                    0x58 == get_bytes(stream, i + 7, buffer, buf_len) &&
                    0x00 == get_bytes(stream, i + 8, buffer, buf_len) &&
                    0x10 == get_bytes(stream, i + 9, buffer, buf_len)) {
                    jpeg_thumb_in_stream = 1;
                }
            }

            /* EOI */
            if (((i + 1) < stream_length) && 0xD9 == get_bytes(stream, i + 1, buffer, buf_len)) {
                *p_offset = i + 2;
                /* update amount of thumbnail or full resolution image */
                if (jpeg_thumb_in_stream) {
                    jpeg_thumb_in_stream = 0;
                } else
                    return 0;
            }
        }
    }

    return -1;
}
#ifdef READ_FRAME_OPTIMIZE
static unsigned int find_next_start_code(unsigned char *buffer, unsigned int *buffer_size, struct stream_context *ctx, uint32_t *zero_count, off_t nal_begin, off_t *sync_offset) {
    int ret;
    uint32_t i ;
    int tmp_zero_count = 0;
    unsigned char *input_buf;
    unsigned int len;
    uint32_t valid_len;
    uint32_t boundary_off = ctx->type == BIT_STREAM_H264 || ctx->type == BIT_STREAM_AVS2 ? 1 : 2;
    off_t begin;
    begin = ctx->buffer_type == BUFFER_NO_USE ? 0 : nal_begin + *zero_count + 1;
    while(1) {
        input_buf = buffer;
        len = ctx->buffer_data_len;
        for (i = begin; i < len; i++) {
            if (input_buf[i] == 0) {
                tmp_zero_count++;
            } else if (input_buf[i] == 1 && tmp_zero_count >= 2) {
                break;
            } else {
                tmp_zero_count = 0;
            }
        }


        //update buffer data; nal in buffer reserves at least one or two bytes for type checking
        if (i >= len - boundary_off) {
            if (ctx->eof == 0) {
                valid_len = len - ctx->buffer_offset;
                if (valid_len == len) {
                    LOG_WARN("Insufficient buffer size %d, for file <%s>",*buffer_size, ctx->path);
                    if (*buffer_size == MAX_STREAM_BUFFER_SIZE) {
                       LOG_ERROR("Insufficient buffer size %d, for file <%s>",*buffer_size, ctx->path);
                       return -1; 
                    } 
                    *buffer_size *=2;
                    buffer = (unsigned char *) realloc(buffer, *buffer_size);
                    ctx->buffer = buffer;
                    input_buf = buffer;
                    len = *buffer_size;
                } else {
                    memmove(buffer, buffer + ctx->buffer_offset, valid_len);
                }
                ret = fread(buffer + valid_len, 1, len - valid_len, ctx->file);
                if (feof(ctx->file)) {
                    ctx->eof = 1;
                    ctx->offset = ftello(ctx->file);
                }
                ctx->buffer_data_len = valid_len + ret;
                ctx->buffer_offset = 0;
                if (input_buf[valid_len - 1] == 1) {
                    //Avoid buffer end is 00 00 00 01 ,research sync word
                    begin = valid_len - 1;
                } else {
                    begin = valid_len;
                }
            } else {
                tmp_zero_count = 0;
                break;
            }
            
        } else {
            break;
        } 
    }
    

    *zero_count = tmp_zero_count > 3 ? 3 : tmp_zero_count;
    *sync_offset = i - *zero_count;
    return 0;
}

#else
static off_t find_next_start_code(struct stream_context *ctx, uint32_t *zero_count)
{
    off_t start = ftello(ctx->file);
    *zero_count = 0;
    /* Scan for the beginning of the packet. */
    for (int i = 0; i < ctx->size && i < ctx->size - start; i++) {
        unsigned char byte;
        int ret_val = fgetc(ctx->file);
        if (ret_val == EOF)
            return ftello(ctx->file);
        byte = (unsigned char)ret_val;
        switch (byte) {
        case 0:
            *zero_count = *zero_count + 1;
            break;
        case 1:
            /* If there's more than three leading zeros, consider only three
             * of them to be part of this packet and the rest to be part of
             * the previous packet. */
            if (*zero_count > 3)
                *zero_count = 3;
            if (*zero_count >= 2) {
                return ftello(ctx->file) - *zero_count - 1;
            }
            *zero_count = 0;
            break;
        default:
            *zero_count = 0;
            break;
        }
    }
    return ftello(ctx->file);
}
#endif

#ifdef READ_FRAME_OPTIMIZE
static uint32_t check_au_boundary(unsigned char *buffer, off_t nal_begin, int type, uint32_t *p_has_slice_data)
{
    uint32_t is_boundary = BS_NO_BOUNDARY;
    uint32_t nal_type, val;
    uint32_t has_slice_data = *p_has_slice_data;
    uint32_t i = type == BIT_STREAM_HEVC || type == BIT_STREAM_AVS2 ? nal_begin + 1 : nal_begin;
    if (type == BIT_STREAM_HEVC) {
        nal_type = (buffer[i] & 0x7E) >> 1;
        if (nal_type > NAL_SLICE_CRA) {
            if (has_slice_data && nal_type != NAL_HEVC_SUFFIX_SEI)
                is_boundary = BS_BOUNDARY;
            else if (nal_type != NAL_HEVC_VPS && nal_type != NAL_HEVC_SPS &&
                     nal_type != NAL_HEVC_PPS && nal_type != NAL_HEVC_PREFIX_SEI &&
                     nal_type != NAL_HEVC_AUD)
                is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
            else
                is_boundary = BS_NO_BOUNDARY;

        } else {
            val = buffer[i+2];
            /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
            if (val & 0x80) {
                is_boundary = BS_BOUNDARY;
                if (!has_slice_data) {
                    is_boundary = BS_NO_BOUNDARY;
                    *p_has_slice_data = 1;
                }
            }
        }
    } else if (type == BIT_STREAM_H264) {
        nal_type = (buffer[i] & 0x1F);
        if (nal_type > NAL_SLICE_IDR) {
            if (has_slice_data)
                is_boundary = BS_BOUNDARY;
            else if (nal_type != NAL_H264_SPS && nal_type != NAL_H264_PPS &&
                     nal_type != NAL_H264_AUD && nal_type != NAL_H264_SEI)
                is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
            else
                is_boundary = BS_NO_BOUNDARY;
        } else {
            val = buffer[i+1];
            /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
            if (val & 0x80) {
                is_boundary = BS_BOUNDARY;
                if (!has_slice_data) {
                    is_boundary = BS_NO_BOUNDARY;
                    *p_has_slice_data = 1;
                }
            }
        }
    } else if (type == BIT_STREAM_AVS2) {
        nal_type = buffer[i];
        if (nal_type == SLICE_START_CODE_MIN) {
            is_boundary = BS_NO_BOUNDARY;
        } else if (nal_type <= SLICE_START_CODE_MAX) {
            is_boundary = BS_NO_BOUNDARY;
        } else {
            is_boundary = BS_NO_BOUNDARY;
            if (nal_type < I_PICTURE_START_CODE) {
                if (has_slice_data)
                    is_boundary = BS_BOUNDARY;
            }
            if (nal_type == I_PICTURE_START_CODE ||
                nal_type == PB_PICTURE_START_CODE) {
                is_boundary = BS_BOUNDARY;
                if (!has_slice_data) {
                    is_boundary = BS_NO_BOUNDARY;
                    *p_has_slice_data = 1;
                }
            }
        }
    }

#if 0
    if (nal_type > (type == BIT_STREAM_HEVC ? NAL_SLICE_CRA : NAL_SLICE_IDR)) {
        is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
    } else {
        if (type == BIT_STREAM_HEVC)
            val = getc(file); // nothing interesting here...
        val = getc(file);
        /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
        if (val & 0x80)
            is_boundary = BS_BOUNDARY;
    }
#endif
    return is_boundary;
}

#else
static uint32_t check_au_boundary(FILE *file, off_t nal_begin, int type, uint32_t *p_has_slice_data)
{
    uint32_t is_boundary = BS_NO_BOUNDARY;
    uint32_t nal_type, val;
    uint32_t has_slice_data = *p_has_slice_data;

    off_t start = ftello(file);
    fseeko(file, type == BIT_STREAM_HEVC ? nal_begin + 1 : nal_begin, SEEK_SET);
    if (type == BIT_STREAM_HEVC) {
        nal_type = (getc(file) & 0x7E) >> 1;
        if (nal_type > NAL_SLICE_CRA) {
            if (has_slice_data && nal_type != NAL_HEVC_SUFFIX_SEI)
                is_boundary = BS_BOUNDARY;
            else if (nal_type != NAL_HEVC_VPS && nal_type != NAL_HEVC_SPS &&
                     nal_type != NAL_HEVC_PPS && nal_type != NAL_HEVC_PREFIX_SEI &&
                     nal_type != NAL_HEVC_AUD)
                is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
            else
                is_boundary = BS_NO_BOUNDARY;

        } else {
            val = getc(file); // nothing interesting here...
            val = getc(file);
            /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
            if (val & 0x80) {
                is_boundary = BS_BOUNDARY;
                if (!has_slice_data) {
                    is_boundary = BS_NO_BOUNDARY;
                    *p_has_slice_data = 1;
                }
            }
        }
    } else if (type == BIT_STREAM_H264) {
        nal_type = (getc(file) & 0x1F);
        if (nal_type > NAL_SLICE_IDR) {
            if (has_slice_data)
                is_boundary = BS_BOUNDARY;
            else if (nal_type != NAL_H264_SPS && nal_type != NAL_H264_PPS &&
                     nal_type != NAL_H264_AUD && nal_type != NAL_H264_SEI)
                is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
            else
                is_boundary = BS_NO_BOUNDARY;
        } else {
            val = getc(file);
            /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
            if (val & 0x80) {
                is_boundary = BS_BOUNDARY;
                if (!has_slice_data) {
                    is_boundary = BS_NO_BOUNDARY;
                    *p_has_slice_data = 1;
                }
            }
        }
    }

#if 0
    if (nal_type > (type == BIT_STREAM_HEVC ? NAL_SLICE_CRA : NAL_SLICE_IDR)) {
        is_boundary = BS_BOUNDARY_NON_SLICE_NAL;
    } else {
        if (type == BIT_STREAM_HEVC)
            val = getc(file); // nothing interesting here...
        val = getc(file);
        /* Check if first slice segment in picture(or first mb in slice is 0(ue(v)) ) */
        if (val & 0x80)
            is_boundary = BS_BOUNDARY;
    }
#endif
    fseeko(file, start, SEEK_SET);
    return is_boundary;
}
#endif 

static int read_jpeg(struct stream_context *ctx)
{
    unsigned int ret;
    unsigned char *stream_p;
    unsigned int stream_len;

    /* read input stream from file to buffer and close input file */
    ret = fread(ctx->buffer, sizeof(unsigned char), ctx->size, ctx->file);
    if ((off_t)ret < ctx->size) {
        ctx->offset = ftello(ctx->file);
        if (feof(ctx->file)) {
            ctx->eof = 1;
            LOG_INFO("End of stream for <%s>", ctx->path);
        }

        if (ret <= 0)
            return 0;
    }

    stream_p = ctx->buffer;
    ret = find_image_eoi(stream_p, ret, &stream_len, ctx->buffer, ctx->buffer_size);
    if (ret != 0) {
        LOG_WARN("EOI missing from end of file!");
    }
    return stream_len;
}

static enum RdrFileFormat FfCheckFormat(FILE* fin, int *type) {
  char id[5] = "DKIF";
  char string[5] = "";
  enum RdrFileFormat  format = BIT_STREAM_UNKNOWN;

  if (fread(string, 1, 5, fin) == 5)
  {
    if (!strncmp(id, string, 5)) {
      fseeko(fin, 8, SEEK_SET);
      if (!fread(string, 1, 4, fin)) return 0;
        if (!strncasecmp("VP90", string, 4)) {
        format = BIT_STREAM_VP9_IVF;
        *type = BIT_STREAM_VP9;
      } else if (!strncasecmp("AV01", string, 4)) {
        format = BIT_STREAM_AV1_IVF;
        *type = BIT_STREAM_AV1;
      }
    }
  }
  fseeko(fin, 0, SEEK_SET);
  return format;
}

stream_context_ptr stream_open(const char *file_name, int type)
{
    stream_context_ptr ctx = NULL;
    uint32_t buffer_size = DEFAULT_STREAM_BUFFER_SIZE;


    if (!file_name) {
        LOG_ERROR("Invalid parameters for opening file %p, type %d", file_name, type);
        goto fail;
    }

    ctx = (stream_context_ptr)malloc(sizeof(struct stream_context));
    memset(ctx, 0, sizeof(struct stream_context));
    ctx->file = fopen(file_name, "rb");
    if (!ctx->file) {
        LOG_ERROR("File to open file <%s>, type %d", file_name, type);
        goto fail;
    }

    if (type == BIT_STREAM_AV1 || type == BIT_STREAM_VP9) {
        char* suffix = NULL;
        suffix = strrchr(file_name,'.');
        if (suffix != NULL) {
            if (!strcmp(suffix, ".av1")) {
                ctx->format = BIT_STREAM_AV1_AV1;
            } else if (!strcmp(suffix, ".obu")) {
                ctx->format = BIT_STREAM_AV1_OBU;
            } else if (!strcmp(suffix, ".ivf")) {
                ctx->format = FfCheckFormat(ctx->file, &type);
            } else if (!strcmp(suffix, ".avif")) {
                ctx->format = BIT_STREAM_AV1_AVIF;
            }
        }
        if (ctx->format == BIT_STREAM_UNKNOWN) {
            LOG_ERROR("unknown or not supported bit format.");
            goto fail;
        }
    }

    ctx->type = type;
    fseeko(ctx->file, 0, SEEK_END);
    ctx->size = ftello(ctx->file);
    fseeko(ctx->file, 0, SEEK_SET);

    if (type == BIT_STREAM_JPEG)
        buffer_size =
            ctx->size > DEFAULT_STREAM_BUFFER_SIZE ? ctx->size : DEFAULT_STREAM_BUFFER_SIZE;

    ctx->buffer = malloc(buffer_size);
    if (!ctx->buffer) {
        LOG_ERROR("File to malloc buffer for %s, size %d", file_name, buffer_size);
        goto fail;
    }
    ctx->buffer_size = buffer_size;

    memcpy(ctx->path, file_name, strlen(file_name));

    return ctx;

fail:
    if (ctx && ctx->file)
        fclose(ctx->file);
    if (ctx && ctx->buffer)
        free(ctx->buffer);
    if (ctx)
        free(ctx);
    return NULL;
}

void stream_close(stream_context_ptr *pctx)
{
    stream_context_ptr ctx;
    if (pctx && *pctx) {
        ctx = *pctx;
        if (ctx->file)
            fclose(ctx->file);
        if (ctx->buffer)
            free(ctx->buffer);
        if (ctx)
            free(ctx);
        *pctx = NULL;
    }
}

static int leb128(const unsigned char *p, int *len) {
  int s = 0;
  for (int i = 0; i < 8; i++) {
    unsigned char b = *p++;
    s |= ((uint64_t)(b&0x7f)) << (i * 7);
    if (!(b & 0x80)) {
      *len = i+1;
      break;
    }
  }
  return s;
}

static int ReadIvfFileHeader(FILE* fin, IVF_HEADER *ivf) {
  uint32_t tmp;

  tmp = fread(ivf, sizeof(char), sizeof(IVF_HEADER), fin);
  if (tmp == 0) return -1;

  return 0;
}

static int ReadIvfFrameHeader(FILE* fin, uint32_t* frame_size) {
  union {
    IVF_FRAME_HEADER ivf;
    uint8_t p[12];
  } fh;
  uint32_t tmp;

  tmp = fread(&fh, sizeof(char), sizeof(IVF_FRAME_HEADER), fin);
  if (tmp == 0) return -1;

  *frame_size = fh.p[0] + (fh.p[1] << 8) + (fh.p[2] << 16) + (fh.p[3] << 24);

  return 0;
}

static int ReadObuHeader(const unsigned char* data_start, const unsigned char* data_end, obuHeader_t* hdr,
                  int annexb, int annexb_size, int size_check) {
  const unsigned char* local = data_start;
  if (local == data_end)
    return 1;
  unsigned char b0 = *local++;
  if (b0 & 1) {
    // Forbidden bit. Must not be set.
    return 1;
  }
  hdr->type           = (b0 >> 3) & 0xf;
  hdr->has_extension  = (b0 >> 2) & 1;
  hdr->has_size_field = (b0 >> 1) & 1;

  if (!hdr->has_size_field && !annexb) {
    // section 5 obu streams must have obu_size field set.
    return 1;
  }

  if (b0 >> 7) {
    // obu_reserved_1bit must be set to 0.
    return 1;
  }

  if (hdr->has_extension) {
    if (local == data_end)
      return 1;
    unsigned char b1 = *local++;
    if (b1 & 0x7) {
      // extension_header_reserved_3bits must be set to 0.
      return 1;
    }
    hdr->temporal_layer_id = (b1 >> 5) & 0x7;
    hdr->spatial_layer_id  = (b1 >> 3) & 0x3;
  }

  int size = 0;
  if(hdr->has_size_field) {
    int l = 0;
    size = leb128(local, &l);
    if (size < 0 || (size_check && size > (data_end - local)))
      return 1;
    local += l;
  } else {
    if (!annexb) return 1;
    size = annexb_size - (hdr->has_extension ? 2 : 1);
  }

  hdr->header_size  = local - data_start;
  hdr->total_size   = size + hdr->header_size;
  hdr->payload_size = size;

  return 0;
}

static int read_vp9_av1(unsigned char *buffer, unsigned int *buffer_size, struct stream_context *ctx)
{
    unsigned int stream_len;

    unsigned int tmp;
    off_t frame_header_pos;
    unsigned int frame_size = 0;
    FILE* fin = ctx->file;

  
    

    /* Read VPx IVF file header */
    if ((ctx->format == BIT_STREAM_VP9_IVF || ctx->format == BIT_STREAM_AV1_IVF) &&
        !ctx->ivf_headers_read) {
        tmp = ReadIvfFileHeader(fin, &ctx->ivf_header);
        if (tmp != 0) return tmp;
        ctx->ivf_headers_read = 1;
    }

    frame_header_pos = ftello(fin);
    if (ctx->format == BIT_STREAM_VP9_IVF || ctx->format == BIT_STREAM_AV1_IVF) {
        tmp = ReadIvfFrameHeader(fin, &frame_size);
        if (tmp != 0) {
            if (feof(fin)) {
                LOG_INFO("End of stream for <%s>", ctx->path);
                ctx->eof = 1;
                return 0;
            }
            return tmp;
        }
    } else if (ctx->format == BIT_STREAM_AV1_OBU) {
        // plain obus
        unsigned char tmpbuf[10];
        // read until next temporal delimiter
        int first = 1;
        while (1) {
            obuHeader_t hdr = {0};
            // max size of hdr + size field is 10
            size_t tmp = fread(tmpbuf, sizeof(unsigned char), 10, fin);
            if (tmp) {
                if (ReadObuHeader(tmpbuf, tmpbuf + tmp, &hdr, 0, 0, 0)) {
                    LOG_ERROR("reading OBU header failed");
                    return -1;
                }
            }
            // OBU_TEMPORAL_DELIMITER == 2
            if ((!first && hdr.type == 2) || tmp == 0) {
                if (tmp == 0 && first) {
                    if (feof(fin)) {
                        LOG_INFO("End of stream for <%s>", ctx->path);
                        ctx->eof = 1;
                        return 0;
                    }
                }
                fseeko(fin, -tmp, SEEK_CUR);
                break;
            }
            fseeko(fin, hdr.total_size - tmp, SEEK_CUR);
            frame_size += hdr.total_size;
            first = 0;
        }
        fseeko(fin, -(size_t)frame_size, SEEK_CUR);
    }

    if (feof(fin)) {
        LOG_INFO("End of stream for <%s>", ctx->path);
        ctx->eof = 1;
        return 0;
    }

    if (frame_size > *buffer_size) {
        LOG_INFO("End of stream for <%s>", ctx->path);
        fprintf(stderr, "Frame size %d > buffer size %d\n", frame_size, *buffer_size);
        fseeko(fin, frame_header_pos, SEEK_SET);
        *buffer_size = frame_size;
        return -1;
    }

    tmp = fread((unsigned char*)buffer, sizeof(unsigned char), frame_size, fin);
    stream_len = frame_size;
    
    return stream_len;
}


#ifdef READ_FRAME_OPTIMIZE
int stream_read_frame(stream_context_ptr ctx, uint8_t **pdata)
{
    if (!ctx || !pdata) {
        LOG_ERROR("Invalide parameters ctx %p, pdata %p", ctx, pdata);
        return -1;
    }

    if (ctx->type == BIT_STREAM_JPEG) {
        ctx->buffer_offset = 0;
        *pdata = ctx->buffer;
        return read_jpeg(ctx);
    } else if (ctx->type == BIT_STREAM_AV1 || ctx->type == BIT_STREAM_VP9) {
        ctx->buffer_offset = 0;
        *pdata = ctx->buffer;
        return read_vp9_av1((unsigned char *)ctx->buffer, &ctx->buffer_size, ctx);
    }

    off_t begin, end, strm_len;
    uint32_t zero_count = 0;
    uint32_t tmp, ret = 0;
    uint32_t bytes_off = ctx->type == BIT_STREAM_H264;
    uint32_t boundary_off = ctx->type == BIT_STREAM_H264 ? 1 : 2;
    uint32_t has_slice_data = 0;
    off_t nal_begin;

    if (ctx->buffer_type == BUFFER_NO_USE) {
        ret = fread(ctx->buffer, 1, ctx->buffer_size, ctx->file);
        if (feof(ctx->file)) {
            ctx->offset = ftello(ctx->file);
            ctx->eof = 1;
            if (ret == 0) {
                LOG_INFO("End of stream for <%s>", ctx->path);
                *pdata = NULL;
                return 0;
            }
        }
        ctx->buffer_data_len = ret;
        if (find_next_start_code(ctx->buffer, &ctx->buffer_size, ctx, &zero_count, 0, &begin) > 0) {
            return -1;
        }

        ctx->buffer_offset = begin;
        ctx->buffer_zero_count = zero_count;
        ctx->buffer_type = BUFFER_IN_USE;
    }

    if (ctx->eof && ctx->buffer_offset == (off_t)ctx->buffer_data_len) {
        *pdata = NULL;
        return 0;
    }

    nal_begin = ctx->buffer_offset;
    zero_count = ctx->buffer_zero_count;
    tmp = check_au_boundary(ctx->buffer, nal_begin + zero_count + bytes_off, ctx->type, &has_slice_data);
    if (find_next_start_code(ctx->buffer,&ctx->buffer_size,ctx,&zero_count,nal_begin, &nal_begin) > 0) {
        return -1;
    }
    end = nal_begin;
    if (end < (off_t)(ctx->buffer_data_len - boundary_off)  && tmp != BS_BOUNDARY_NON_SLICE_NAL) {
        do {
            end = nal_begin;
            if (nal_begin + zero_count + bytes_off >= (off_t)ctx->buffer_data_len) {
                break;
            }
            /* Check access unit boundary for next NAL */
            tmp = check_au_boundary(ctx->buffer, nal_begin + zero_count + bytes_off, ctx->type, &has_slice_data);
            if (tmp == BS_NO_BOUNDARY){
                if (find_next_start_code(ctx->buffer,&ctx->buffer_size,ctx,&zero_count,nal_begin, &nal_begin) > 0) {
                    return -1;
                }
                if (end == (off_t)ctx->buffer_data_len)
                    break;
            }
            else if (tmp == BS_BOUNDARY_NON_SLICE_NAL) {
                do {
                    if (find_next_start_code(ctx->buffer,&ctx->buffer_size,ctx,&zero_count,nal_begin, &nal_begin) > 0) {
                        return -1;
                    }
                    if (end == nal_begin)
                        break;

                    end = nal_begin;
                    tmp = check_au_boundary(ctx->buffer, nal_begin + zero_count + bytes_off, ctx->type, &has_slice_data);
                } while (tmp == BS_BOUNDARY_NON_SLICE_NAL);

                if (end == nal_begin)
                    break;
                else if (tmp == BS_NO_BOUNDARY)
                    if (find_next_start_code(ctx->buffer,&ctx->buffer_size,ctx,&zero_count,nal_begin, &nal_begin) > 0) {
                        return -1;
                    }
            }
        } while ( tmp != BS_BOUNDARY);
    }


    if (end == ctx->buffer_offset) {
        LOG_INFO("End of stream for <%s>", ctx->path);
        ctx->offset = ftello(ctx->file);
        ctx->eof = 1;

        *pdata = NULL;
        return 0;
    }
    ctx->buffer_zero_count = zero_count;
    strm_len = end - ctx->buffer_offset;

    *pdata = ctx->buffer + ctx->buffer_offset;
    ctx->buffer_offset = end;
    return strm_len;
}
#else
int stream_read_frame(stream_context_ptr ctx, uint8_t **pdata)
{
    if (!ctx || !pdata) {
        LOG_ERROR("Invalide parameters ctx %p, pdata %p", ctx, pdata);
        return -1;
    }

    if (ctx->type == BIT_STREAM_JPEG) {
        *pdata = ctx->buffer;
        return read_jpeg(ctx);
    }

    off_t begin, end, strm_len;
    uint32_t zero_count = 0;
    uint32_t tmp = 0;
    uint32_t strm_read_len;
    uint32_t bytes_off = ctx->type == BIT_STREAM_H264;
    uint32_t has_slice_data = 0;
    off_t nal_begin;

    begin = find_next_start_code(ctx, &zero_count);
    nal_begin = begin + zero_count + bytes_off;
    tmp = check_au_boundary(ctx->file, nal_begin, ctx->type, &has_slice_data);
    end = nal_begin = find_next_start_code(ctx, &zero_count);
    if (end != begin && tmp != BS_BOUNDARY_NON_SLICE_NAL) {
        do {
            end = nal_begin;
            nal_begin += zero_count + bytes_off;

            /* Check access unit boundary for next NAL */
            tmp = check_au_boundary(ctx->file, nal_begin, ctx->type, &has_slice_data);
            if (tmp == BS_NO_BOUNDARY)
                nal_begin = find_next_start_code(ctx, &zero_count);
            else if (tmp == BS_BOUNDARY_NON_SLICE_NAL) {
                do {
                    nal_begin = find_next_start_code(ctx, &zero_count);
                    if (end == nal_begin)
                        break;

                    end = nal_begin;
                    nal_begin += zero_count + bytes_off;
                    tmp = check_au_boundary(ctx->file, nal_begin, ctx->type, &has_slice_data);
                } while (tmp == BS_BOUNDARY_NON_SLICE_NAL);

                if (end == nal_begin)
                    break;
                else if (tmp == BS_NO_BOUNDARY)
                    nal_begin = find_next_start_code(ctx, &zero_count);
            }
        } while (tmp != BS_BOUNDARY);
    }

    if (end == begin) {
        LOG_INFO("End of stream for <%s>", ctx->path);
        ctx->offset = ftello(ctx->file);
        ctx->eof = 1;
        return 0;
    }

    fseeko(ctx->file, begin, SEEK_SET);
    
    if (ctx->buffer_size < end - begin) {
        LOG_WARN("Insufficient buffer size %d, data length %d, for file <%s>", ctx->buffer_size,
                (uint32_t)(end - begin), ctx->path);
        ctx->offset = ftello(ctx->file);
        return -1;
    }
    strm_len = end - begin;
    strm_read_len = fread(ctx->buffer, 1, strm_len, ctx->file);
    ctx->offset = ftello(ctx->file);
    *pdata = ctx->buffer;

    return strm_read_len;
}
#endif

int raw_open(const char *file_name, vmppPixelFormat fmt, int width, int height, int stride,
             struct raw_context *ctx)
{
    if (!file_name || !ctx) {
        LOG_ERROR("Invalid parameters for opening file %p, ctx %p", file_name, ctx);
        return -1;
    }

    if (fmt != vmpp_PIX_FMT_NV12 && fmt != vmpp_PIX_FMT_NV21 && fmt != vmpp_PIX_FMT_YUV420P &&
        fmt != vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE &&
        fmt != vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010 && fmt != vmpp_PIX_FMT_RGBA) {
        LOG_ERROR("Sorry, format %d is not supported yet!", fmt);
        return -1;
    }
    int comp1_size, comp2_size, comp3_size;
    memset(ctx, 0, sizeof(struct raw_context));
    ctx->file = fopen(file_name, "rb");
    if (ctx->file == NULL) {
        LOG_ERROR("File to open file <%s>", file_name);
        return -1;
    }

    fseeko(ctx->file, 0, SEEK_END);
    ctx->size = ftello(ctx->file);
    fseeko(ctx->file, 0, SEEK_SET);
    memcpy(ctx->path, file_name, strlen(file_name));
    ctx->format = fmt;
    ctx->width = width;
    ctx->height = height;
    ctx->stride[0] = stride;

    switch (ctx->format) {
    case vmpp_PIX_FMT_YUV420P:
        comp1_size = stride * height;
        ctx->stride[1] = stride / 2;
        ctx->stride[2] = stride / 2;
        comp2_size = ctx->stride[1] * height / 2;
        comp3_size = ctx->stride[2] * height / 2;
        break;
    case vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE:
        comp1_size = stride * height * 2;
        ctx->stride[1] = stride / 2;
        ctx->stride[2] = stride / 2;
        comp2_size = ctx->stride[1] * height;
        comp3_size = ctx->stride[2] * height;
        break;
    case vmpp_PIX_FMT_NV12:
    case vmpp_PIX_FMT_NV21:
        comp1_size = stride * height;
        ctx->stride[1] = stride;
        comp2_size = ctx->stride[1] * height / 2;
        comp3_size = 0;
        break;
    case vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010:
        comp1_size = stride * height * 2;
        ctx->stride[1] = stride;
        comp2_size = ctx->stride[1] * height;
        comp3_size = 0;
        break;
    case vmpp_PIX_FMT_RGBA:
        comp1_size = stride * height * 4;
        comp2_size = 0;
        comp3_size = 0;
        ctx->stride[1] = 0;
        ctx->stride[2] = 0;
        break;
    default:
        comp1_size = 0;
        comp2_size = comp3_size = 0;
        break;
    }
    ctx->comp1_size = comp1_size;
    ctx->comp2_size = comp2_size;
    ctx->comp3_size = comp3_size;
    ctx->pic_size = comp1_size + comp2_size + comp3_size;
    return 0;
}

void raw_close(struct raw_context *ctx)
{
    if (ctx && ctx->file) {
        fclose(ctx->file);
        memset(ctx, 0, sizeof(struct raw_context));
    }
}

int raw_pic_size(const struct raw_context *ctx, int *comp1, int *comp2, int *comp3)
{
    if (!ctx || !comp1 || !comp2 || !comp3)
        return 0;
    *comp1 = ctx->comp1_size;
    *comp2 = ctx->comp2_size;
    *comp3 = ctx->comp3_size;
    return ctx->pic_size;
}

int raw_read_frame(struct raw_context *ctx, vmppFrame *frame)
{
    if (!ctx || !frame || !frame->data[0]) {
        LOG_ERROR("error 1!");
        return -1;
    }
    int ret = 0;

    if (ctx->format == vmpp_PIX_FMT_NV12 || ctx->format == vmpp_PIX_FMT_NV21 ||
        ctx->format == vmpp_PIX_FMT_YUV420P ||
        ctx->format == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE ||
        ctx->format == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010) {
        ret = fread(frame->data[0], 1, ctx->comp1_size, ctx->file);
        if (ret <= 0) {
            goto fail;
        }

        ret = fread(frame->data[1], 1, ctx->comp2_size, ctx->file);
        if (ret <= 0) {
            goto fail;
        }

        if (ctx->comp3_size != 0) {
            ret = fread(frame->data[2], 1, ctx->comp3_size, ctx->file);
            if (ret <= 0) {
                goto fail;
            }
        }
        frame->pixelFormat = ctx->format;
    } else if (ctx->format == vmpp_PIX_FMT_RGBA) {
        ret = fread(frame->data[0], 1, ctx->pic_size, ctx->file);
        if (ret <= 0) {
            goto fail;
        }
    }
    frame->dataSize = ctx->pic_size;
    frame->width = ctx->width;
    frame->height = ctx->height;
    frame->stride[0] = ctx->stride[0];
    frame->stride[1] = ctx->stride[1];
    frame->stride[2] = ctx->stride[2];
    frame->memoryType = vmpp_MEM_HOST;
    frame->pixelFormat = ctx->format;
    return ret;
fail:
    if (feof(ctx->file))
        ctx->eof = 1;
    else
        LOG_WARN("Read data failed.");
    return ret;
}

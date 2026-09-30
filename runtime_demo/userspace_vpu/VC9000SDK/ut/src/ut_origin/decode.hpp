#ifdef __cplusplus
extern "C"
{
#endif
// #include "basetype.h"

// #include "va_vdata_internal.h"
#include "vmpp_dec_defs.h"
#include <pthread.h>
#include <semaphore.h>

// #include "hevc_decapi.h"

// #ifndef JPEGDECCONT_H
// #define JPEGDECCONT_H
// #include "jpegdecapi.h"
// #include "deccfg.h"
// #include "decppif.h"
// #include "vpufeature.h"
// #include "decapicommon.h"
// #include "ppu.h"
// #include "input_queue.h"


#define VA_MAX_OUTPUT_BUFFER (64+8)
#define VA_MAX_PTS_BUFFER (VA_MAX_OUTPUT_BUFFER * 2)
#define VA_MAX_SEI_BUFFER (VA_MAX_OUTPUT_BUFFER * 4)

/** Maximum number of cores supported in multi-core configuration */
/** For G2, multi-core currently not supported. */
#define MAX_ASIC_CORES 5 //3*2

/* Common defines for the decoder */
#define DEC_MAX_PPU_COUNT 5

#define VIDEO_MAX_BUFFERS (16 + 40 + 8 + 8) /* 16 is max GOP size, 40 is for encoder lookahead, 8 is for encoder max GOP size, another 8 is for pass1.*/

struct va_priv_buf {
    uint8_t *private_data;
    uint32_t used;
};

struct va_pts_buf {
    int64_t pts;
    uint64_t flag;
};

struct va_sei_buf {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
};

struct va_sei_params {
    vmppSEI *sei_data;
    uint32_t used;
    uint64_t pts;
    void *privateData;
};

typedef unsigned int u32;

/**
 * struct for video codec channel context
 */
struct va_dec_channel {
    vmppVersion version;
    void *codec_inst; /* codec instance, decoder/encoder for jpeg/h264/hevc */
    volatile uint32_t state;
    const void *cwl;       /* codec wrapper layer, dwl/ewl */
    void *private_context; /* private context for decoder/encoder */

    // vmppRuntimeInstance *runtime_inst;
    // uint32_t tmv_enable;          /* temporal motion vector predictors can be used for inter
    // prediction */

    vmppDecChannelParameters params;

    uint32_t extraBufferused; /* extra buffer used */
    uint32_t is_ringbuffer;   /* ring buffer mode by default */
    uint32_t tile_by_tile;
    uint32_t frame_struct_size;

    uint32_t max_buf_num;
    struct va_priv_buf private_buffer[VA_MAX_OUTPUT_BUFFER];
    struct va_pts_buf frame_pts_buf[VA_MAX_PTS_BUFFER];
    pthread_mutex_t private_buffer_mutex;

    struct va_sei_buf sei_buffer[VA_MAX_SEI_BUFFER];
    struct va_sei_params va_sei_parameters[VA_MAX_SEI_BUFFER];
    pthread_mutex_t sei_buffer_mutex;

    volatile uint32_t receive_frame_cnt;
    volatile uint32_t release_frame_cnt;
};

/*!\struct H264CropParams_
 * \brief Picture cropping information
 *
 * \typedef H264CropParams
 * A typename for #H264CropParams_.
 */
typedef struct H264CropParams_ {
  u32 crop_left_offset;
  u32 crop_out_width;
  u32 crop_top_offset;
  u32 crop_out_height;
} H264CropParams;

/* Output picture format types */
enum DecPictureFormat {
  DEC_OUT_FRM_TILED_4X4 = 0,
  DEC_OUT_FRM_TILED_8X4 = 1,
  DEC_OUT_FRM_RASTER_SCAN = 2, /* a.k.a. SEMIPLANAR_420 */
  DEC_OUT_FRM_PLANAR_420 = 3,
  DEC_OUT_FRM_MONOCHROME,       /* a.k.a. YUV400 */
  DEC_OUT_FRM_RFC,
  DEC_OUT_FRM_RGB,
  DEC_OUT_FRM_DEC400,
  /* YUV420 */
  DEC_OUT_FRM_YUV420TILE,        /* YUV420, 8-bit, Tile4x4 */
  DEC_OUT_FRM_YUV420TILE_PACKED, /* Reference frame format */
  DEC_OUT_FRM_YUV420TILE_P010,
  DEC_OUT_FRM_YUV420TILE_1010,
  DEC_OUT_FRM_YUV420SP,          /* YUV420, 8-bit, semi-planar */
  DEC_OUT_FRM_YUV420SP_PACKED,
  DEC_OUT_FRM_YUV420SP_P010,
  DEC_OUT_FRM_YUV420SP_1010,
  DEC_OUT_FRM_YUV420SP_I010,
  DEC_OUT_FRM_YUV420P,            /* YUV420, 8-bit, planar */
  DEC_OUT_FRM_YUV420P_PACKED,
  DEC_OUT_FRM_YUV420P_P010,
  DEC_OUT_FRM_YUV420P_1010,
  DEC_OUT_FRM_YUV420P_I010,
   /* YUV400 */
  DEC_OUT_FRM_YUV400TILE,        /* YUV420, 8-bit, Tile4x4 */
  DEC_OUT_FRM_YUV400TILE_P010,
  DEC_OUT_FRM_YUV400TILE_1010,
  DEC_OUT_FRM_YUV400,            /* YUV420, 8-bit, planar */
  DEC_OUT_FRM_YUV400_P010,
  DEC_OUT_FRM_YUV400_1010,
  /* NV21 */
  DEC_OUT_FRM_NV21TILE,        /* YUV420, 8-bit, Tile4x4 */
  DEC_OUT_FRM_NV21TILE_PACKED, /* Reference frame format */
  DEC_OUT_FRM_NV21TILE_P010,
  DEC_OUT_FRM_NV21TILE_1010,
  DEC_OUT_FRM_NV21SP,          /* YUV420, 8-bit, semi-planar */
  DEC_OUT_FRM_NV21SP_PACKED,
  DEC_OUT_FRM_NV21SP_P010,
  DEC_OUT_FRM_NV21SP_1010,
  DEC_OUT_FRM_NV21P,            /* YUV420, 8-bit, planar */
  DEC_OUT_FRM_NV21P_PACKED,
  DEC_OUT_FRM_NV21P_P010,
  DEC_OUT_FRM_NV21P_1010,
  /* RGB */
  DEC_OUT_FRM_RGB888,
  DEC_OUT_FRM_BGR888,
  DEC_OUT_FRM_R16G16B16,
  DEC_OUT_FRM_B16G16R16,
  DEC_OUT_FRM_RGB888_P,
  DEC_OUT_FRM_BGR888_P,
  DEC_OUT_FRM_R16G16B16_P,
  DEC_OUT_FRM_B16G16R16_P,
  DEC_OUT_FRM_ARGB888,
  DEC_OUT_FRM_ABGR888,
  DEC_OUT_FRM_A2R10G10B10,
  DEC_OUT_FRM_A2B10G10R10,
  DEC_OUT_FRM_XRGB888,
  DEC_OUT_FRM_XBGR888
};

typedef struct H264DecInfo_ {
  u32 pic_width;        /**< decoded picture width in pixels */
  u32 pic_height;       /**< decoded picture height in pixels */
  u32 video_range;      /**< samples' video range */
  u32 colour_primaries;
  u32 transfer_characteristics;
  u32 colour_description_present_flag; /* indicate matrix_coefficients/
                      colour_primaries/transfer_characteristics present or not */
  u32 matrix_coefficients;
  H264CropParams crop_params;  /**< display cropping information */
  enum DecPictureFormat output_format;  /**< format of the output picture */
  u32 sar_width;        /**< sample aspect ratio */
  u32 sar_height;       /**< sample aspect ratio */
  u32 mono_chrome;      /**< is sequence monochrome */
  u32 interlaced_sequence;      /**< is sequence interlaced */
  u32 dpb_mode;         /**< DPB mode; frame, or field interlaced */
  u32 pic_buff_size;     /**< number of picture buffers allocated and used by decoder */
  u32 multi_buff_pp_size; /**< number of picture buffers needed in decoder+postprocessor multibuffer mode */
  u32 bit_depth;
  u32 pp_enabled;
  u32 base_mode;  /**< decode this stream in non-high10 mode and not use ringbuffer*/
} H264DecInfo;

typedef enum {
  DEC_ALIGN_1B = 0,
  DEC_ALIGN_8B = 3,
  DEC_ALIGN_16B,
  DEC_ALIGN_32B,
  DEC_ALIGN_64B,
  DEC_ALIGN_128B,
  DEC_ALIGN_256B,
  DEC_ALIGN_512B,
  DEC_ALIGN_1024B,
  DEC_ALIGN_2048B,
} DecPicAlignment;

typedef struct _PpUnitConfig {
  u32 enabled;    /* PP unit enabled */
  u32 tiled_e;    /* PP unit tiled4x4 output enabled */
  u32 rgb;        /* RGB output enabled */
  u32 rgb_planar; /* RGB output planar output enabled */
  u32 cr_first;   /* CrCb instead of CbCr */
  u32 shaper_enabled;
  u32 shaper_no_pad;
  u32 dec400_enabled; /* sw control shaper and dec400  */
  u32 planar;     /* Planar output */
  DecPicAlignment align;  /* pp output alignment */
  /* Stride for Y/C plane. SW should use the stride calculated from SW if it's
     set to 0. When not 0, SW should check the validation of the value. */
  u32 ystride;
  u32 cstride;
  struct {
    u32 enabled;  /* whether cropping is enabled */
    u32 set_by_user;   /* cropping set by user, use this variable to record
                        * whether user set crop.*/
    u32 x;        /* cropping start x */
    u32 y;        /* cropping start y */
    u32 width;    /* cropping width */
    u32 height;   /* cropping height */
  } crop;
  struct {
    u32 enabled;
    u32 x;        /* cropping start x */
    u32 y;        /* cropping start y */
    u32 width;    /* cropping width */
    u32 height;   /* cropping height */
  } crop2;
  struct {
    u32 enabled;  /* whether scaling is enabled */
    u32 set_by_user;   /* scaling set by user, use this variable to record
                        * whether user set scale.*/
    u32 ratio_x;  /* 0 indicate flexiable mode, or 1/2/4/8 indicate ratio */
    u32 ratio_y;
    u32 width;    /* scaled output width */
    u32 height;   /* scaled output height */
  } scale;
  u32 monochrome; /* PP output monochrome (luma only) for YUV output */
  u32 out_p010;
  u32 out_1010;
  u32 out_I010;
  u32 out_L010;
  u32 out_be;
  u32 out_cut_8bits;
  u32 video_range;  /* 1 - full range, 0 - limited range */
  u32 range_max;
  u32 range_min;
  u32 out_format;
  u32 rgb_format;   /* RGB output format: RGB888/BGR888/R16G16B16/... */
  u32 rgb_stan;     /* color conversion standard applied to set coeffs */
  u32 rgb_alpha;
  u32 pp_filter;
  u32 x_filter_param;
  u32 y_filter_param;
  u32 afbc_mode;
} PpUnitConfig;

#ifdef __FREERTOS__
typedef unsigned long long addr_t; //Now the FreeRTOS Simulator just support the 64bit env
#else
// typedef size_t addr_t;
typedef unsigned long long addr_t;
#endif
// typedef size_t ptr_t;
typedef unsigned long long ptr_t;

typedef enum {
  DWL_MEM_MALLOC_BOTH       = 0x1,
  DWL_MEM_MALLOC_HOST_ONLY  = 0x2,
  DWL_MEM_MALLOC_DEV_ONLY   = 0x4,
  DWL_MEM_MALLOC_NULL       = 0xE,
} DWLMemMallocType;

/* Linear memory area descriptor */
struct DWLLinearMem {
  u32 *virtual_address;
  addr_t bus_address;
  u32 size;         /* physical size (rounded to page multiple) */
  u32 logical_size; /* requested size in bytes */
  u32 mem_type;
#ifdef SUPPORT_MMU
  addr_t unmap_bus_address; /* used when free buffer*/
#endif
  u32 is_ref;

  u32 *allocVirtualAddr;   /**< allocated virtural address access by CPU */
  ptr_t allocBusAddr;     /**< allocated bus address access by HW */

  DWLMemMallocType mallocType;
};

/*!\brief Stream consumed callback prototype
 *
 * This callback is invoked by the decoder to notify the application that
 * a stream buffer was fully processed and can be reused.
 *
 * \param stream base address of a buffer that was set as input when
 *                calling H264DecDecode().
 * \param p_user_data application provided pointer to some private data.
 *                  This is set at decoder initialization time.
 *
 * \sa H264DecMCInit();
 */
typedef void H264DecMCStreamConsumed(void *stream, void *p_user_data);

/*!\struct H264DecMCConfig_
 * \brief Multicore decoder init configuration
 *
 * \typedef H264DecMCConfig
 *  A typename for #H264DecMCConfig_.
 */
typedef struct H264DecMCConfig_ {
  u32 mc_enable;
  /*! Application provided callback for stream buffer processed. */
  H264DecMCStreamConsumed *stream_consumed_callback;

} H264DecMCConfig;

enum DelogoMode {
  PIXEL_NO_DELOGO = 0,
  PIXEL_REPLACE = 1,
  PIXEL_INTERPOLATION = 2
};

typedef struct _DelogoConfig {
  u32 enabled;
  u32 x;
  u32 y;
  u32 w;
  u32 h;
  u32 show;
  enum DelogoMode mode;
  u32 Y;
  u32 U;
  u32 V;
} DelogoConfig;

/* error handling */
enum DecErrorHandling {
  /* Data property */
  DEC_EC_PIC_COPY_REF = 0x1,       /* Copy whole data from reference picture buffer to current picture buffer. */
  DEC_EC_PIC_PARTIAL = 0x2,        /* Copy partial picture data from reference picture buffer to current picture buffer. */
  DEC_EC_PIC_PARTIAL_IGNORE = 0x4, /* Ignore partial invalid data in current picture buffer */
  DEC_EC_PIC_ALL_IGNORE = 0x8,     /* Do nothing for the data in current picture buffer */
  /* Reference property */
  DEC_EC_REF_REPLACE = 0x100,      /* If one reference is erroneous, relpace it with nearest correct ref picture in POC distance. */
  DEC_EC_REF_NEXT_IDR = 0x200,     /* If one reference is erroneous, mark current picture as erroneous directly until a new IDR picture encountered. */
  DEC_EC_REF_NEXT_I = 0x400,       /* If one reference is erroneous, mark current picture as erroneous directly until a new I picture encountered. */
  /* Output property */
  DEC_EC_OUT_ALL = 0x10000,            /* Output all pictures including the picture marked as erroneous picture. */
  DEC_EC_OUT_NO_ERROR = 0x20000,       /* Output correct pictures, other pictures are discarded. */
  DEC_EC_OUT_FIRST_FIELD_OK = 0x40000, /* Output correct pictures and the pictures contain correct 1st field, other pictures are discarded. */
  /* Typical combinations */
  DEC_EC_PICTURE_FREEZE = DEC_EC_PIC_COPY_REF,                       /* If current picture is erroneous, freeze current picture */
  DEC_EC_VIDEO_FREEZE = (DEC_EC_PIC_COPY_REF | DEC_EC_REF_NEXT_IDR), /* If current picture is erroneous, freeze whole picture until a new IDR picture encountered. */
  DEC_EC_PARTIAL_FREEZE = DEC_EC_PIC_PARTIAL,                        /* If current picture is erroneous in partial space , freeze partial picture */
  DEC_EC_PARTIAL_IGNORE = DEC_EC_PIC_PARTIAL_IGNORE,                 /* If current picture is erroneous in partial space, remove the erroneous flag and treat it as correct picture */
  DEC_EC_FAST_FREEZE = (DEC_EC_PIC_ALL_IGNORE | DEC_EC_REF_NEXT_I | DEC_EC_OUT_NO_ERROR)  /* If current picture is erroneous, mark all pictures as erroneous until a new I picture encountered, and discard erroneous pictures */
};

/* DPB flags to control reference picture format etc. */
enum DecDpbFlags {
  /* Reference frame formats */
  DEC_REF_FRM_RASTER_SCAN = 0x0,
  DEC_REF_FRM_TILED_DEFAULT = 0x1,

  /* Flag to allow SW to use DPB field ordering on interlaced content */
  DEC_DPB_ALLOW_FIELD_ORDERING = 0x40000000
};

/* Decoder working mode */
enum DecDecoderMode {
  DEC_NORMAL =           0x00000000,
  DEC_LOW_LATENCY =      0x00000001,
  DEC_LOW_LATENCY_RTL =  0x00000002,
  DEC_SECURITY =         0x00000004,
  DEC_PARTIAL_DECODING = 0x00000008,
  DEC_INTRA_ONLY =       0x00000010
};

struct H264DecConfig {
  u32 no_output_reordering;
  enum DecErrorHandling error_handling;
  u32 use_video_compressor;
  u32 use_ringbuffer;
  u32 use_display_smoothing;
  enum DecDpbFlags dpb_flags;
  enum DecDecoderMode decoder_mode;
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  u32 fixed_scale_enabled;
  DecPicAlignment align;
  u32 error_conceal;
  PpUnitConfig ppu_config[DEC_MAX_PPU_COUNT];
  DelogoConfig delogo_params[2];
  H264DecMCConfig mcinit_cfg;
  u32 rlc_mode;
  u32 user_output_buf;
  u32 min_dev_ppbuf;
  u32 user_as_ppbuf;
  u32 low_delay;
  u32 serial_call_enable;
  u32 no_BFrame;
};

struct h264_decoder_private_context {
    uint32_t pic_decode_number;
    uint32_t pic_display_number;
    uint32_t headers_ready;
    uint32_t pic_ready;
    H264DecInfo dec_info;
    struct H264DecConfig dec_cfg;
    PpUnitConfig ppu_cfg[DEC_MAX_PPU_COUNT];
    struct DWLLinearMem ext_buffers[VIDEO_MAX_BUFFERS];
    uint32_t ext_buffers_number;
    uint32_t buffer_size;
    H264DecInfo last_dec_info;
};


/* Output picture pixel format types for raster scan or down scale output */
enum DecPicturePixelFormat {
  DEC_OUT_PIXEL_DEFAULT = 0,    /* packed pixel: each pixel in at most 10 bits as reference buffer */
  DEC_OUT_PIXEL_P010 = 1,       /* a.k.a. MS P010 format */
  DEC_OUT_PIXEL_CUSTOMER1 = 2,  /* customer format: a 128-bit burst output in packed little endian format */
  DEC_OUT_PIXEL_CUT_8BIT = 3,   /* cut 10 bit to 8 bit per pixel */
  DEC_OUT_PIXEL_RFC = 4,        /* compressed tiled output */
  DEC_OUT_PIXEL_1010 = 5,
};

/*!\brief Stream consumed callback prototype
 *
 * This callback is invoked by the decoder to notify the application that
 * a stream buffer was fully processed and can be reused.
 *
 * \param stream base address of a buffer that was set as input when
 *                calling HevcDecDecode().
 * \param p_user_data application provided pointer to some private data.
 *                  This is set at decoder initialization time.
 */
typedef void HevcDecMCStreamConsumed(void *stream, void *p_user_data);

/* brief Multicore decoder init configuration */
struct HevcDecMCConfig {
  u32 mc_enable;
  /*! Application provided callback for stream buffer processed. */
  HevcDecMCStreamConsumed *stream_consumed_callback;

};

struct HevcDecConfig {
  u32 no_output_reordering;
  u32 use_video_freeze_concealment;
  u32 use_video_compressor;
  u32 use_ringbuffer;
  u32 tile_by_tile;
  enum DecDecoderMode decoder_mode;
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  u32 fixed_scale_enabled;
  DecPicAlignment align;
#if 0
  struct {
    u32 enabled;  // whether cropping is enabled
    u32 x;        // cropping start x
    u32 y;        // cropping start y
    u32 width;    // cropping width
    u32 height;   // cropping height
  } crop;
  struct {
    u32 enabled;  // whether scaling is enabled
    u32 width;    // scaled output width
    u32 height;   // scaled output height
  } scale;
#else
  PpUnitConfig ppu_cfg[DEC_MAX_PPU_COUNT];
#endif
  DelogoConfig delogo_params[2];
  enum DecPictureFormat output_format;
  enum DecPicturePixelFormat pixel_format;
  struct HevcDecMCConfig mcinit_cfg;
  u32 user_output_buf;
  u32 min_dev_ppbuf;
  u32 user_as_ppbuf;
  u32 low_delay;
  u32 serial_call_enable;
  u32 buf_slim_mode;
};

/* cropping info */
struct HevcCropParams {
  u32 crop_left_offset;
  u32 crop_out_width;
  u32 crop_top_offset;
  u32 crop_out_height;
};

/* stream info filled by HevcDecGetInfo */
struct HevcDecInfo {
  u32 pic_width;   /* decoded picture width in pixels */
  u32 pic_height;  /* decoded picture height in pixels */
  u32 video_range; /* samples' video range */
  u32 matrix_coefficients;
  u32 colour_primaries; /* indicates the chromaticity coordinates of the source primaries */
  struct HevcCropParams crop_params;   /* display cropping information */
  enum DecPictureFormat output_format; /* format of the output picture */
  enum DecPicturePixelFormat pixel_format; /* format of the pixels in output picture */
  u32 sar_width;                       /* sample aspect ratio */
  u32 sar_height;                      /* sample aspect ratio */
  u32 mono_chrome;                     /* is sequence monochrome */
  u32 interlaced_sequence;             /* is sequence interlaced */
  u32 dpb_mode;      /* DPB mode; frame, or field interlaced */
  u32 pic_buff_size; /* number of picture buffers allocated&used by decoder */
  u32 multi_buff_pp_size; /* number of picture buffers needed in
                             decoder+postprocessor multibuffer mode */
  u32 bit_depth;     /* bit depth per pixel stored in memory */
  u32 pic_stride;         /* Byte width of the pixel as stored in memory */

  /* for HDR */
  u32 transfer_characteristics;
};

struct hevc_decoder_private_context {
    uint32_t pic_decode_number;
    uint32_t headers_ready;
    uint32_t min_buffer_number;
    uint32_t ext_buffer_number;
    uint32_t prev_width;
    uint32_t prev_height;
    uint32_t prev_buf_width;
    uint32_t prev_buf_height;
    uint32_t buffer_size;
    PpUnitConfig ppu_cfg[DEC_MAX_PPU_COUNT];
    struct HevcDecConfig dec_cfg;
    struct HevcDecInfo dec_info;
    struct DWLLinearMem ext_buffers[VIDEO_MAX_BUFFERS];
    volatile uint32_t buffer_consumed[VIDEO_MAX_BUFFERS];
    pthread_mutex_t buffer_mutex;
    uint32_t resolution_changed;
};

#ifdef __cplusplus
}
#endif
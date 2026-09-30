/*
 * Copyright (c) 2022, Vastai Tech. All rights reserved
 *
 * The information contained herein is confidential
 * property of Company. The user, copying, transfer or
 * disclosure of such information is prohibited except
 * by express written agreement with VASTAITECH.
 */

#include "video_encoder.h"
#include "va_log.h"
#include "va_vdata_internal.h"
#include "vmpp_enc_defs.h"
#include "enc_helper.h"
#include <math.h>

#define VMPP_ENABLE_ROI_MAP 0
#define VMPP_BUFFER_CNT_FOR_REORDER 8
#define ALLIGN(size, alignment) (((size) + ((alignment)-1)) & (~((alignment)-1)))

#define ALIGN_4K(addr) (((addr) + 0xFFF) & ~0xFFF)
#define CALC_ALIGNED_ADDR(ptr, base, offset, page_mask) do { \
    base = (uint8_t*)((uintptr_t)(ptr) & page_mask); \
    offset = (uint8_t*)(ptr) - base; \
} while(0)

#define VMPP_OUTBUFFER_IDLE_NUM 2
#define VMPP_OUTBUFFER_MALLOC_NUM 4
#define VMPP_OUTBUFFER_RELEASE_NUM 2

/*------------------------------------------------------------------------------
    Compatibility definitions for the encoder API provided by this SDK. The
    symbols below are not part of the shipped hevcencapi.h, they are either
    renamed or not supported by the prebuilt library, so the corresponding
    features are disabled here.
------------------------------------------------------------------------------*/
#define SINGLE_CORE_MODE 0
#define MULTI_CORE_MODE 1

/* VMAF tuning is not supported, the closest tuning mode is used instead. */
#define VCENC_TUNE_VMAF VCENC_TUNE_VISUAL
/* CRA is not supported, an intra frame is used instead. */
#define VCENC_CRA_FRAME VCENC_INTRA_FRAME
/* VCEncIn has no insertIDR field in this version, bIsIDR is used instead. */
#define insertIDR bIsIDR

#define VCENC_MIN_ENC_WIDTH 176
#define VCENC_MAX_ENC_WIDTH 8192
#define VCENC_MIN_ENC_HEIGHT 144
#define VCENC_MAX_ENC_HEIGHT 8192
#define VCENC_STREAM_MAX_BUF0_SIZE VCENC_STREAM_MIN_BUF0_SIZE

/* Values that are never returned by the library, the related code is disabled. */
#define VCENC_NO_CHANNEL ((VCEncRet)-100)
#define VCENC_HASH_LEN_MISMATCH ((VCEncRet)-101)

// Type POC QPoffset  QPfactor TemporalId num_ref_pics ref_pics  used_by_cur
static char *RpsDefault_GOPSize_1[] = {
    "Frame1:  P    1   0        0.578     0      1        -1         1",
    NULL,
};

static char *RpsDefault_V60_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8     0      1        -1         1",
    NULL,
};

static char *RpsDefault_H264_GOPSize_1[] = {
    "Frame1:  P    1   0        0.5     0      1        -1         1",
    NULL,
};

char *RpsDefault_H264_multicore_GOPSize_1[] = {
    "Frame1:  P        1   0        0.4     0      1        -1         1",
    NULL,
};

char *RpsDefault_H264_GOPSize_4[] = {
    "Frame1:  P        4   0        0.35    0      1        -4         1 ",
    "Frame2:  B        2   2        0.25    0      2        -2 2       1 1",
    "Frame3:  nrefB    1   3        0.3     0      3        -1 1 3     1 1 0",
    "Frame4:  nrefB    3   3        0.3     0      2        -1 1       1 1 ",
    NULL,
};

char *RpsDefault_H264_GOPSize_8[] = {
    "Frame1:  P        8   0        0.3     0      1        -8         1 ",
    "Frame2:  B        4   2        0.3     0      2        -4 4       1 1 ",
    "Frame3:  B        2   2        0.45    0      3        -2 2 6     1 1 0 ",
    "Frame4:  nrefB    1   3        0.65    0      4        -1 1 3 7   1 1 0 0",
    "Frame5:  nrefB    3   3        0.65    0      3        -1 1 5     1 1 0",
    "Frame6:  B        6   2        0.35    0      2        -2 2       1 1",
    "Frame7:  nrefB    5   3        0.55    0      3        -1 1 3     1 1 0",
    "Frame8:  nrefB    7   3        0.55    0      2        -1 1       1 1",
    NULL,
};

static char *RpsDefault_GOPSize_2[] = {
    "Frame1:  P        2   0        0.6     0      1        -2         1",
    "Frame2:  nrefB    1   2        0.68    0      2        -1 1       1 1",
    NULL,
};

static char *RpsDefault_GOPSize_3[] = {
    "Frame1:  P        3   0        0.5     0      1        -3         1   ",
    "Frame2:  B        1   2        0.5     0      2        -1 2       1 1 ",
    "Frame3:  nrefB    2   3        0.68    0      2        -1 1       1 1 ",
    NULL,
};

static char *RpsDefault_GOPSize_4[] = {
    "Frame1:  P        4   0        0.5      0     1       -4         1 ",
    "Frame2:  B        2   2        0.3536   0     2       -2 2       1 1",
    "Frame3:  nrefB    1   3        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  nrefB    3   3        0.5      0     2       -1 1       1 1 ",
    NULL,
};

static char *RpsDefault_GOPSize_5[] = {
    "Frame1:  P        5   0        0.442    0     1       -5         1 ",
    "Frame2:  B        2   2        0.3536   0     2       -2 3       1 1",
    "Frame3:  nrefB    1   3        0.68     0     3       -1 1 4     1 1 0",
    "Frame4:  B        3   2        0.3536   0     2       -1 2       1 1 ",
    "Frame5:  nrefB    4   3        0.68     0     2       -1 1       1 1 ",
    NULL,
};

static char *RpsDefault_GOPSize_6[] = {
    "Frame1:  P        6   0        0.442    0     1       -6         1 ",
    "Frame2:  B        3   2        0.3536   0     2       -3 3       1 1",
    "Frame3:  B        1   2        0.3536   0     3       -1 2 5     1 1 0",
    "Frame4:  nrefB    2   3        0.68     0     3       -1 1 4     1 1 0",
    "Frame5:  B        4   2        0.3536   0     2       -1 2       1 1 ",
    "Frame6:  nrefB    5   3        0.68     0     2       -1 1       1 1 ",
    NULL,
};

static char *RpsDefault_GOPSize_7[] = {
    "Frame1:  P        7   0        0.442    0     1       -7         1 ",
    "Frame2:  B        3   2        0.3536   0     2       -3 4       1 1",
    "Frame3:  B        1   2        0.3536   0     3       -1 2 6     1 1 0",
    "Frame4:  nrefB    2   3        0.68     0     3       -1 1 5     1 1 0",
    "Frame5:  B        5   2        0.3536   0     2       -2 2       1 1 ",
    "Frame6:  nrefB    4   3        0.68     0     3       -1 1 3     1 1 0",
    "Frame7:  nrefB    6   3        0.68     0     2       -1 1       1 1 ",
    NULL,
};

static char *RpsDefault_GOPSize_8[] = {
    "Frame1:  P        8   0        0.442    0  1           -8        1 ",
    "Frame2:  B        4   2        0.3536   0  2           -4 4      1 1 ",
    "Frame3:  B        2   2        0.3536   0  3           -2 2 6    1 1 0 ",
    "Frame4:  nrefB    1   3        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  nrefB    3   3        0.68     0  3           -1 1 5    1 1 0",
    "Frame6:  B        6   2        0.3536   0  2           -2 2      1 1",
    "Frame7:  nrefB    5   3        0.68     0  3           -1 1 3    1 1 0",
    "Frame8:  nrefB    7   3        0.68     0  2           -1 1      1 1",
    NULL,
};

static char *RpsDefault_GOPSize_16[] = {
    "Frame1:  P       16   0        0.6      0  1           -16                   1",
    "Frame2:  B        8   0        0.2      0  2           -8   8                1   1",
    "Frame3:  B        4   0        0.33     0  3           -4   4  12            1   1   0",
    "Frame4:  B        2   0        0.33     0  4           -2   2   6  14        1   1   0   0",
    "Frame5:  nrefB    1   0        0.4      0  5           -1   1   3   7  15    1   1   0   0   "
    "0",
    "Frame6:  nrefB    3   0        0.4      0  4           -1   1   5  13        1   1   0   0",
    "Frame7:  B        6   0        0.33     0  3           -2   2  10            1   1   0",
    "Frame8:  nrefB    5   0        0.4      0  4           -1   1   3  11        1   1   0   0",
    "Frame9:  nrefB    7   0        0.4      0  3           -1   1   9            1   1   0",
    "Frame10: B       12   0        0.33     0  2           -4   4                1   1",
    "Frame11: B       10   0        0.33     0  3           -2   2   6            1   1   0",
    "Frame12: nrefB    9   0        0.4      0  4           -1   1   3   7        1   1   0   0",
    "Frame13: nrefB   11   0        0.4      0  3           -1   1   5            1   1   0",
    "Frame14: B       14   0        0.33     0  2           -2   2                1   1",
    "Frame15: nrefB   13   0        0.4      0  3           -1   1   3            1   1   0",
    "Frame16: nrefB   15   0        0.4      0  2           -1   1                1   1",
    NULL,
};

static char *RpsDefault_SVC_GOPSize_2[] = {
    "Frame1:  P        1   0        0.68    1  1           -1        1 ",
    "Frame2:  P        2   0        0.6     0  1           -2        1 ",
    NULL,
};

static char *RpsDefault_SVC_GOPSize_4[] = {
    "Frame1:  P        1   0        0.5       2  1           -1        1 ",
    "Frame2:  P        2   0        0.3536    1  1           -2        1 ",
    "Frame3:  P        3   0        0.5       2  2           -1 -3     1 0 ",
    "Frame4:  P        4   0        0.5       0  1           -4        1 ",
    NULL,
};

static char *RpsDefault_SVC_GOPSize_8[] = {
    "Frame1:  P        1   0        0.68      3  1           -1        1 ",
    "Frame2:  P        2   0        0.3536    2  1           -2        1 ",
    "Frame3:  P        3   0        0.68      3  2           -1 -3     1 0 ",
    "Frame4:  P        4   0        0.3536    1  1           -4        1",
    "Frame5:  P        5   0        0.68      3  2           -1 -5     1 0 ",
    "Frame6:  P        6   0        0.3536    2  2           -2 -6     1 0",
    "Frame7:  P        7   0        0.68      3  2           -1 -7     1 0 ",
    "Frame8:  P        8   0        0.442     0  1           -8        1",
    NULL,
};

static char *RpsDefault_SVC_P2B_GOPSize_2[] = {
    "Frame1:  B        1   0        0.68     1  2           -1 -3        1 1 ",
    "Frame2:  B        2   0        0.6      0  2           -2 -4        1 1 ",
    NULL,
};

static char *RpsDefault_SVC_P2B_GOPSize_4[] = {
    "Frame1:  nrefB        1   0        0.5       2  2           -1 -5       1 1 ",
    "Frame2:  B            2   0        0.3536    1  2           -2 -6       1 1 ",
    "Frame3:  nrefB        3   0        0.5       2  3           -1 -3 -7    1 1 0 ",
    "Frame4:  B            4   0        0.5       0  2           -4 -8       1 1 ",
    NULL,
};

static char *RpsDefault_SVC_P2B_GOPSize_8[] = {
    "Frame1:  nrefB        1   0        0.68      3  2           -1 -9           1 1 ",
    "Frame2:  B            2   0        0.3536    2  2           -2 -10          1 1 ",
    "Frame3:  nrefB        3   0        0.68      3  3           -1 -3 -11       1 1 0 ",
    "Frame4:  B            4   0        0.3536    1  2           -4 -12          1 1 ",
    "Frame5:  nrefB        5   0        0.68      3  3           -1 -5 -13       1 1 0 ",
    "Frame6:  B            6   0        0.3536    2  3           -2 -6 -14       1 1 0 ",
    "Frame7:  nrefB        7   0        0.68      3  4           -1 -3 -7 -15    1 1 0 0 ",
    "Frame8:  B            8   0        0.442     0  2           -8 -16          1 1 ",
    NULL,
};

#if 0
static char *Rps_LongTerm_Int24[] = {
    "Frame1:  B 1 0 0.4624 0     2  -1 L1  1 1",
    "Frame2:  B 2 0 0.4624 0     2  -1 L1  1 1",
    "Frame3:  B 3 0 0.4624 0     2  -1 L1  1 1",
    "Frame4:  B 4 0 0.578  0     2  -1 L1  1 1",
    "Frame0: -255 -255 -255 -255 2  -1 L1  1 1  1 0 24",
    NULL,
};
#endif

static char *RpsDefault_Interlace_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8       0   2           -1 -2     0 1",
    NULL,
};

static char *RpsLowdelayDefault_GOPSize_1[] = {
    "Frame1:  B    1   0        0.65      0     2       -1 -2         1 1",
    NULL,
};

static char *RpsLowdelayDefault_GOPSize_2[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -3         1 1",
    "Frame2:  B    2   0        0.578     0     2       -1 -2         1 1",
    NULL,
};

static char *RpsLowdelayDefault_GOPSize_3[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -4         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.578     0     2       -1 -3         1 1",
    NULL,
};

static char *RpsLowdelayDefault_GOPSize_4[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -5         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.4624    0     2       -1 -3         1 1",
    "Frame4:  B    4   0        0.578     0     2       -1 -4         1 1",
    NULL,
};

static char *RpsPass2_GOPSize_4[] = {
    "Frame1:  B        4   0        0.5      0     2       -4 -8      1 1",
    "Frame2:  B        2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  nrefB    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  nrefB    3   0        0.5      0     3       -1 -3 1    1 0 1",
    NULL,
};

static char *RpsPass2_GOPSize_8[] = {
    "Frame1:  B        8   0        0.442    0  2           -8 -16    1 1",
    "Frame2:  B        4   0        0.3536   0  2           -4 4      1 1",
    "Frame3:  B        2   0        0.3536   0  3           -2 2 6    1 1 0",
    "Frame4:  nrefB    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  nrefB    3   0        0.68     0  4           -1 -3 1 5 1 0 1 0",
    "Frame6:  B        6   0        0.3536   0  3           -2 -6 2   1 0 1",
    "Frame7:  nrefB    5   0        0.68     0  4           -1 -5 1 3 1 0 1 0",
    "Frame8:  nrefB    7   0        0.68     0  3           -1 -7 1   1 0 1",
    NULL,
};

static char *RpsPass2_GOPSize_2[] = {
    "Frame1:  B        2   0        0.6     0      2        -2 -4      1 1",
    "Frame2:  nrefB    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

static char *RpsPass2_GOPSize_1[] = {
    "Frame1:  B        1   0        0.578     0      2        -1 -2      1 1",
    NULL,
};

char *NonRefB_GOPSize_3[] = {
    "Frame1:  P        3   0        0.5     0      1        -3         1   ",
    "Frame2:  nrefB    1   2        0.68    0      2        -1 2       1 1 ",
    "Frame3:  nrefB    2   2        0.68    0      2        -2 1       1 1 ",
    NULL,
};

char *NonRefB_GOPSize_4[] = {
    "Frame1:  P        4   0        0.4      0     1       -4         1 ",
    "Frame2:  nrefB    1   2        0.5      0     2       -1 3       1 1",
    "Frame3:  nrefB    2   2        0.5      0     2       -2 2       1 1",
    "Frame4:  nrefB    3   2        0.5      0     2       -3 1       1 1",
    NULL,
};

char *NonRefB_GOPSize_5[] = {
    "Frame1:  P        5   0        0.442    0     1       -5         1 ",
    "Frame2:  nrefB    1   2        0.68     0     2       -1 4       1 1",
    "Frame3:  nrefB    2   2        0.68     0     2       -2 3       1 1",
    "Frame4:  nrefB    3   2        0.68     0     2       -3 2       1 1",
    "Frame5:  nrefB    4   2        0.68     0     2       -4 1       1 1",
    NULL,
};

char *NonRefB_GOPSize_6[] = {
    "Frame1:  P        6   0        0.442    0     1       -6         1 ",
    "Frame2:  nrefB    1   2        0.68     0     2       -1 5       1 1",
    "Frame3:  nrefB    2   2        0.68     0     2       -2 4       1 1",
    "Frame4:  nrefB    3   2        0.68     0     2       -3 3       1 1",
    "Frame5:  nrefB    4   2        0.68     0     2       -4 2       1 1",
    "Frame6:  nrefB    5   2        0.68     0     2       -5 1       1 1",
    NULL,
};

char *NonRefB_GOPSize_7[] = {
    "Frame1:  P        7   0        0.442    0     1       -7         1 ",
    "Frame2:  nrefB    1   2        0.68     0     2       -1 6       1 1",
    "Frame3:  nrefB    2   2        0.68     0     2       -2 5       1 1",
    "Frame4:  nrefB    3   2        0.68     0     2       -3 4       1 1",
    "Frame5:  nrefB    4   2        0.68     0     2       -4 3       1 1",
    "Frame6:  nrefB    5   2        0.68     0     2       -5 2       1 1",
    "Frame7:  nrefB    6   2        0.68     0     2       -6 1       1 1",
    NULL,
};

char *NonRefB_GOPSize_8[] = {
    "Frame1:  P        8   0        0.442    0  1           -8        1 ",
    "Frame2:  nrefB    1   2        0.68     0  2           -1 7      1 1",
    "Frame3:  nrefB    2   2        0.68     0  2           -2 6      1 1",
    "Frame4:  nrefB    3   2        0.68     0  2           -3 5      1 1",
    "Frame5:  nrefB    4   2        0.68     0  2           -4 4      1 1",
    "Frame6:  nrefB    5   2        0.68     0  2           -5 3      1 1",
    "Frame7:  nrefB    6   2        0.68     0  2           -6 2      1 1",
    "Frame8:  nrefB    7   2        0.68     0  2           -7 1      1 1",
    NULL,
};


// test for LTR
#if 1
/*OK*/
char *LTR_GOPSize_8[] = {
    //# normal frame configure
    //#      Type  POC  QPoffset   QPfactor  TemporalId  num_ref_pics    ref_pics    used_by_cur  

    "Frame1:  P        1   0        0.4624      0  1           -1        1 ",
    "Frame2:  P        2   0        0.4624      0  1           -2        1 ",
    "Frame3:  P        3   0        0.4624      0  2           -1 -3      1 0 ",
    "Frame4:  P        4   0        0.4624      0  1           -4        1 ",
    "Frame5:  P        5   0        0.4624      0  2           -1 -5     1 0 ",
    "Frame6:  P        6   0        0.4624      0  2           -2 -6     1 0",
    "Frame7:  P        7   0        0.4624      0  2           -1 -7     1 0 ",
    "Frame8:  P        8   0        0.4624      0  1           -8        1",
    //# long-term reference and special frame configure
    //#      Type  QPoffset QPfactor   TemporalId  num_ref_pics  ref_pics   used_by_cur   LTR    Offset  Interval 
    //#default  -255      -255      -255         -255                                          
    "Frame0:  I          -2        0.4624       -255         2            -8  L1         0   0      1       0        80",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      2       0        32",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      0       8        8",
    NULL,
};
#else
#if 0
char *LTR_GOPSize_8[] = {
    //# normal frame configure
    //#      Type  POC  QPoffset   QPfactor  TemporalId  num_ref_pics    ref_pics    used_by_cur  

    "Frame1:  P        1   0        0.4624      0  1           -1        1 ",
    "Frame2:  P        2   0        0.4624      0  1           -2        1 ",
    "Frame3:  P        3   0        0.4624      0  2           -1 -3      1 0 ",
    "Frame4:  P        4   0        0.4624      0  1           -4        1 ",
    "Frame5:  P        5   0        0.4624      0  2           -1 -5     1 0 ",
    "Frame6:  P        6   0        0.4624      0  2           -2 -6     1 0",
    "Frame7:  P        7   0        0.4624      0  2           -1 -7     1 0 ",
    "Frame8:  P        8   0        0.4624      0  1           -8        1",
    //# long-term reference and special frame configure
    //#      Type  QPoffset QPfactor   TemporalId  num_ref_pics  ref_pics   used_by_cur   LTR    Offset  Interval 
    //#default  -255      -255      -255         -255                                          
    "Frame0:  I          -2        0.4624       -255         2            -8  L1         0   0      1       0        80",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      2       0        32",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      0       8        8",
    NULL,
};
#else
static char *LTR_GOPSize_8[] = {
    //# normal frame configure
    //#      Type  POC  QPoffset   QPfactor  TemporalId  num_ref_pics    ref_pics    used_by_cur  

    "Frame1:  P        1   0        0.4624      0  1           -1        1 ",
    "Frame2:  P        2   0        0.4624      0  1           -1        1 ",
    "Frame3:  P        3   0        0.4624      0  1           -1        1 ",
    "Frame4:  P        4   0        0.4624      0  1           -1        1 ",
    "Frame5:  P        5   0        0.4624      0  1           -1        1 ",
    "Frame6:  P        6   0        0.4624      0  1           -1        1 ",
    "Frame7:  P        7   0        0.4624      0  1           -1        1 ",
    "Frame8:  P        8   0        0.4624      0  1           -1        1 ",
    //# long-term reference and special frame configure
    //#      Type  QPoffset QPfactor   TemporalId  num_ref_pics  ref_pics   used_by_cur   LTR    Offset  Interval 
    //#default  -255      -255      -255         -255                                          
    "Frame0:  -255       -255      0.4624       -255         1            L1             1          1       0        31",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      2       0        19",
    "Frame0:  -255       -255      0.4624       -255         2            L2  L1         1   1      0       8        8",
    NULL,
};
#endif
#endif

static void clear_out_buffer_list(struct va_enc_channel *chn)
{
    pthread_mutex_lock(&chn->enc_out_buffer_mutex);
    for (uint32_t i = 0; i < chn->outbufNum; i++) {
        if (chn->enc_out_buffer[i].data) {
            LOG_DEBUG(ENC, "clear_data_buffer %d, %p", i, chn->enc_out_buffer[i].data);
            free(chn->enc_out_buffer[i].data);
            chn->enc_out_buffer[i].data = NULL;
            chn->enc_out_buffer[i].size = 0;
        }
        chn->enc_out_buffer[i].used = 0;
    }
    chn->outbufIdleNum = 0;
    chn->outbufMallocNum = 0;
    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
}

static uint8_t *get_idle_out_buffer(struct va_enc_channel *chn, uint32_t size)
{
    uint8_t *priv_buf = NULL;
    uint32_t i = 0;
    uint32_t n = 0;
    int32_t  j = -1;
    pthread_mutex_lock(&chn->enc_out_buffer_mutex);

#if 0
    printf("USER_BUF: get_idle_out_buffer used: ");
    for (uint32_t i = 0; i < chn->outbufNum; i++) {
        printf("%d", chn->enc_out_buffer[i].used);
    }
    printf("\n");
#endif

    n = VMPP_OUTBUFFER_RELEASE_NUM;
    for (j = chn->outbufNum - 1; j >= 0; j--) {
        if (chn->outbufIdleNum > VMPP_OUTBUFFER_IDLE_NUM && chn->outbufMallocNum > VMPP_OUTBUFFER_MALLOC_NUM) {
            if (!chn->enc_out_buffer[j].used && chn->enc_out_buffer[j].data) {
                free(chn->enc_out_buffer[j].data);
                chn->enc_out_buffer[j].data = NULL;
                chn->enc_out_buffer[j].size = 0;
                chn->outbufIdleNum --;
                chn->outbufMallocNum --;
                n--;
                if (n == 0) break;
            }
        }
    }

    for (i = 0; i < chn->outbufNum; i++) {
        if (!chn->enc_out_buffer[i].used)
            break;
    }

    if (i >= chn->outbufNum) {
        LOG_DEBUG(ENC, "No idle private buffer avaliable.");
        pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
        return NULL;
    }

    if (!chn->enc_out_buffer[i].data) {
        chn->enc_out_buffer[i].data = (uint8_t *)malloc(size);
        if (!chn->enc_out_buffer[i].data) {
            LOG_ERROR(ENC, "Fail to malloc private buffer.");
            pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
            return NULL;
        }
        //memset(chn->enc_out_buffer[i].data, 0, size);
        chn->enc_out_buffer[i].size = size;
        chn->outbufMallocNum ++;
    } else {
        if (size > chn->enc_out_buffer[i].size) {
            free(chn->enc_out_buffer[i].data);
            chn->enc_out_buffer[i].data = (uint8_t *)malloc(size);
            if (!chn->enc_out_buffer[i].data) {
                LOG_ERROR(ENC, "Fail to malloc private buffer.");
                pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
                return NULL;
            }
            // memset(chn->enc_out_buffer[i].data, 0, size);
            chn->enc_out_buffer[i].size = size;
        }
        chn->outbufIdleNum --;
    }

    priv_buf = chn->enc_out_buffer[i].data;
    chn->enc_out_buffer[i].used = 1;

    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);
    return priv_buf;
}

static vmppResult set_out_buffer_idle(struct va_enc_channel *chn, uint8_t *data)
{
    uint32_t i = 0;

    if (data == NULL) {
        LOG_ERROR(ENC, "data = NULL!!!");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    pthread_mutex_lock(&chn->enc_out_buffer_mutex);

#if 0
    printf("USER_BUF: set_out_buffer_idle used: ");
    for (uint32_t i = 0; i < chn->outbufNum; i++) {
        printf("%d", chn->enc_out_buffer[i].used);
    }
    printf("\n");
#endif

    for (i = 0; i < chn->outbufNum; i++) {
        if (chn->enc_out_buffer[i].data == data) {
            chn->enc_out_buffer[i].used = 0;
            chn->outbufIdleNum ++;
            break;
        }
    }
    pthread_mutex_unlock(&chn->enc_out_buffer_mutex);

    if (i == chn->outbufNum) {
        LOG_ERROR(ENC, "Can't find the buffer in the outbuf list.");
        return vmpp_RSLT_WARN_STREAM_NOT_EXIST;
    }

    return vmpp_RSLT_OK;
}

static inline VCEncLevel levelPar2Internal(vmppVideoLevel level) { return (VCEncLevel)level; }
static inline VCEncProfile profilePar2Internal(vmppVideoProfile profile)
{
    return (VCEncProfile)profile;
}
static VCEncVideoCodecFormat formatPar2Internal(vmppCodecType type)
{
    if (type == vmpp_CODEC_ENC_HEVC)
        return VCENC_VIDEO_CODEC_HEVC;
    else if (type == vmpp_CODEC_ENC_H264)
        return VCENC_VIDEO_CODEC_H264;
    else if (type == vmpp_CODEC_ENC_AV1)
        return VCENC_VIDEO_CODEC_AV1;
    return VCENC_VIDEO_CODEC_HEVC;
}
static inline VCEncPictureType picformatPar2Internal(vmppPixelFormat fmt)
{
    VCEncPictureType vType = VCENC_YUV420_SEMIPLANAR;
    switch (fmt) {
    case vmpp_PIX_FMT_YUV420P:
        vType = VCENC_YUV420_PLANAR;
        break;
    case vmpp_PIX_FMT_NV12:
        vType = VCENC_YUV420_SEMIPLANAR;
        break;
    case vmpp_PIX_FMT_NV21:
        vType = VCENC_YUV420_SEMIPLANAR_VU;
        break;
    case vmpp_PIX_FMT_RGB24:
        vType = VCENC_RGB888;
        break;
    case vmpp_PIX_FMT_BGR24:
        vType = VCENC_BGR888;
        break;
    case vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE:
        vType = VCENC_YUV420_PLANAR_10BIT_I010;
        break;
    case vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010:
        vType = VCENC_YUV420_PLANAR_10BIT_P010;
        break;
    case vmpp_PIX_FMT_RGBA:
        vType = VCENC_BGR888;
        break;
    case vmpp_PIX_FMT_GRAY8:
    case vmpp_PIX_FMT_ARGB:
    case vmpp_PIX_FMT_ABGR:
    case vmpp_PIX_FMT_BGRA:
    default:
        break;
    }
    return vType;
}

static inline VCENC_TuneType TunePar2Internal(vmppEncTuneType tune)
{
    switch (tune) {
    case vmpp_ENC_TUNE_PSNR:
        return VCENC_TUNE_PSNR;
    case vmpp_ENC_TUNE_SSIM:
        return VCENC_TUNE_SSIM;
    case vmpp_ENC_TUNE_VISUAL:
        return VCENC_TUNE_VISUAL;
    case vmpp_ENC_TUNE_SHARP_VISUAL:
        return VCENC_TUNE_SHARP_VISUAL;
    case vmpp_ENC_TUNE_VMAF:
        return VCENC_TUNE_VMAF;
    default:
        break;
    }
    return VCENC_TUNE_PSNR;
}

/* Selects the RDO level of VCEncConfig and the RDO quant of VCEncCodingCtrl. */
static void Parameter_Preset(VCEncConfig *cfg, uint32_t preset, u32 *enableRdoQuant)
{
    u32 rdoQuant = 0;

    if (preset >= 4) { // gold
        cfg->rdoLevel = 3;
        rdoQuant = 1;
    } else if (preset == 3) { // silver
        if (cfg->codecFormat == VCENC_VIDEO_CODEC_HEVC) {
            cfg->rdoLevel = 1;
            rdoQuant = 1;
        } else {
            cfg->rdoLevel = 2;
            rdoQuant = 0;
        }
    } else if (preset == 1) { // silver_old silverplus_quality
        cfg->rdoLevel = 2;
        rdoQuant = 1;
    } else if (preset == 2) { // bronze only for hevc
        cfg->rdoLevel = 1;
        rdoQuant = 0;
    } else if (preset == 0) {
        cfg->rdoLevel = 1;
        rdoQuant = 0;
    }
    if (cfg->codecFormat == VCENC_VIDEO_CODEC_AV1)
        rdoQuant = 0;
    cfg->rdoLevel -= 1;

    if (enableRdoQuant != NULL)
        *enableRdoQuant = rdoQuant;
}

static void getAlignedPicSizebyFormat(VCEncPictureType type, uint32_t width, uint32_t height,
                                      uint32_t alignment, uint32_t *luma_Size,
                                      uint32_t *chroma_Size, uint32_t *picture_Size)
{
    uint32_t luma_stride = 0, chroma_stride = 0;
    uint32_t lumaSize = 0, chromaSize = 0, pictureSize = 0;

    VCEncGetAlignedStride(width, type, &luma_stride, &chroma_stride, alignment, 0);
    switch (type) {
    case VCENC_YUV420_PLANAR:
    case VCENC_YVU420_PLANAR:
        lumaSize = luma_stride * height;
        chromaSize = chroma_stride * height / 2 * 2;
        break;
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
        lumaSize = luma_stride * height;
        chromaSize = chroma_stride * height / 2;
        break;
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
        lumaSize = luma_stride * height;
        chromaSize = 0;
        break;
    case VCENC_YUV420_PLANAR_10BIT_I010:
        lumaSize = luma_stride * height;
        chromaSize = chroma_stride * height / 2 * 2;
        break;
    case VCENC_YUV420_PLANAR_10BIT_P010:
        lumaSize = luma_stride * height;
        chromaSize = chroma_stride * height / 2;
        break;
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
        lumaSize = luma_stride * 10 / 8 * height;
        chromaSize = chroma_stride * 10 / 8 * height / 2 * 2;
        break;
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
        lumaSize = luma_stride * 2 * 2 * height / 2;
        chromaSize = 0;
        break;
    case VCENC_YUV420_PLANAR_8BIT_TILE_32_32:
        lumaSize = luma_stride * ((height + 32 - 1) & (~(32 - 1)));
        chromaSize = lumaSize / 2;
        break;
    case VCENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
        lumaSize = luma_stride * ((height + 3) / 4);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
        lumaSize = luma_stride * ((height + 3) / 4);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_SEMIPLANAR_101010:
        lumaSize = luma_stride * height;
        chromaSize = chroma_stride * height / 2;
        break;
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
        lumaSize = luma_stride * ((height + 3) / 4);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_10BIT_TILE_32_4:
        lumaSize = luma_stride * ((height + 3) / 4);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
        lumaSize = luma_stride * ((height + 3) / 4);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
        lumaSize = luma_stride * ((height + 1) / 2);
        chromaSize = chroma_stride * (((height / 2) + 1) / 2);
        break;
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
        lumaSize = luma_stride * ((height + 1) / 2);
        chromaSize = chroma_stride * (((height / 2) + 1) / 2);
        break;
    case VCENC_YUV420_8BIT_TILE_8_8:
        lumaSize = luma_stride * ((height + 7) / 8);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    case VCENC_YUV420_10BIT_TILE_8_8:
        lumaSize = luma_stride * ((height + 7) / 8);
        chromaSize = chroma_stride * (((height / 2) + 3) / 4);
        break;
    default:
        LOG_WARN(ENC, "not support this format");
        chromaSize = lumaSize = 0;
        break;
    }

    pictureSize = lumaSize + chromaSize;
    if (luma_Size != NULL)
        *luma_Size = lumaSize;
    if (chroma_Size != NULL)
        *chroma_Size = chromaSize;
    if (picture_Size != NULL)
        *picture_Size = pictureSize;
}

static EWLLinearMem_t *getIdleOutputBuffer(struct video_encoder_private_context *ctx)
{
    int32_t idx = ctx->pictureEncCount % ctx->parallelCoreNum;
    LOG_DEBUG(ENC,
              "getIdleOutputBuffer: %d, ctx->pictureEncCount %d, ctx->parallelCoreNum %d, "
              "busAddress 0x%llx",
              idx, ctx->pictureEncCount, ctx->parallelCoreNum,
              (U64)ctx->outbufMemFactory[idx].busAddress);
    return &ctx->outbufMemFactory[idx];
}

static EWLLinearMem_t *getReadyOutputBuffer(struct video_encoder_private_context *ctx)
{
    int32_t idx = 0;
    if (ctx->inputPictureCount < ctx->parallelCoreNum) {
        if (ctx->cfg.lookaheadDepth == 0)
        {
            idx = ctx->pictureEncCount - ctx->inputPictureCount - 1;
        } else {
            idx = ctx->pictureEncCount - ctx->inputPictureCount - ctx->frameDelay - 1;
        }
    } else {
        idx  = (ctx->pictureEncCount - 1 - (ctx->frameDelay - 1)) % ctx->parallelCoreNum;
    }
    LOG_DEBUG(ENC,
              "getReadyOutputBuffer: %d, ctx->pictureEncCount %d, ctx->frameDelay %d, "
              "ctx->parallelCoreNum %d, busAddress 0x%llx",
              idx, ctx->pictureEncCount, ctx->frameDelay, ctx->parallelCoreNum,
              (U64)ctx->outbufMemFactory[idx].busAddress);
    return &ctx->outbufMemFactory[idx];
}

static int32_t getIdleInputBuffer(struct video_encoder_private_context *ctx,
                                  EncInputBuffer **inputBuffer)
{
    int32_t index = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (!ctx->pictureMem[i].used) {
            LOG_DEBUG(ENC, "getIdleInputBuffer: ctx->bufferCnt %d, index %d, busAddr 0x%llx",
                      ctx->bufferCnt, i, (U64)ctx->pictureMem[i].mem.busAddress);
            ctx->pictureMem[i].used = 1;
            ctx->pictureMem[i].index = i;
            index = i;
            *inputBuffer = &ctx->pictureMem[i];
            break;
        }
    }
    return index;
}

static void setSEIBufferIdle(struct video_encoder_private_context *ctx, uint8_t *buffer);
static void setInputBufferIdle(struct video_encoder_private_context *ctx,
                               EncInputBuffer *inputBuffer)
{
    if (!inputBuffer || inputBuffer->index < 0 || inputBuffer->index >= ctx->bufferCnt) {
        LOG_WARN(ENC, "Invalid input buffer");
        return;
    }
    assert(inputBuffer->index == ctx->pictureMem[inputBuffer->index].index);
    LOG_DEBUG(ENC, "setInputBufferIdle: ctx->bufferCnt %d, index %d-%d, busAddr 0x%llx",
              ctx->bufferCnt, inputBuffer->index, ctx->pictureMem[inputBuffer->index].index,
              (U64)inputBuffer->mem.busAddress);
    ctx->pictureMem[inputBuffer->index].used = 0;
    if (inputBuffer->extSEICount) {
        setSEIBufferIdle(ctx, inputBuffer->extSEI[0].pPayloadData);
        setSEIBufferIdle(ctx, inputBuffer->encodedSEI);
    }
    // Do not free it here, reuse it as much as possible
    // if (inputBuffer->extSEI) {
    //     free(inputBuffer->extSEI);
    //     inputBuffer->extSEI = NULL;
    // }
    inputBuffer->extSEICount = 0;
    inputBuffer->prefixSeiSize = 0;
    inputBuffer->suffixSeiSize = 0;
    inputBuffer->roiMapDeltaQpMem = NULL;
    inputBuffer->roiMapDeltaQpSize = 0;
    inputBuffer->roimapCuCtrlInfoMem = NULL;
    inputBuffer->roimapCuCtrlInfoSize = 0;
    inputBuffer->roimapCuCtrlIndexMem = NULL;
    inputBuffer->roimapCuCtrlIndexSize = 0;
    inputBuffer->orgStreamSize = 0;

    memset(&inputBuffer->roi, 0, sizeof(inputBuffer->roi));
}

static void setInputBufferGopChangeIdr2(struct video_encoder_private_context *ctx, int32_t number)
{
    int lastIPFrmPoc = number > ctx->curIPFramePoc ? ctx->curIPFramePoc : ctx->lastIPFramePoc;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && ctx->pictureMem[i].number < number &&
            ctx->pictureMem[i].number > lastIPFrmPoc) {
            ctx->pictureMem[i].gopChangeIdr = 1;
        }
    }
}

static int32_t setInputBufferGopChangeIdr(struct video_encoder_private_context *ctx)
{
    uint32_t forceIDRNum = -1;
    bool findIDRIdx = false;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].number > ctx->lastIPFramePoc && ctx->pictureMem[i].forceIDR) {
            if ((uint32_t)ctx->pictureMem[i].number < forceIDRNum) {
                forceIDRNum = ctx->pictureMem[i].number;
                findIDRIdx = true;
            }
        }
    }
    if (findIDRIdx)
        setInputBufferGopChangeIdr2(ctx, forceIDRNum);
    return forceIDRNum;
}

// get input buffer index with minimum number?
static int32_t getInputBufferGopChangeIdrIndex(struct video_encoder_private_context *ctx)
{
    int32_t index = -1;
    uint32_t minFrameIndex = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && ctx->pictureMem[i].gopChangeIdr) {

            if ((uint32_t)ctx->pictureMem[i].number < minFrameIndex) {
                minFrameIndex = ctx->pictureMem[i].number;
                index = i;
            }
        }
    }
    return index;
}

static int32_t changeInputBufferGopChangeIdrIndex(struct video_encoder_private_context *ctx, int* gopChangeCnt)
{
    int32_t index = -1;
    uint32_t minFrameIndex = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && ctx->pictureMem[i].gopChangeIdr && !ctx->pictureMem[i].forceIDR) {
            (*gopChangeCnt)++;
            if ((uint32_t)ctx->pictureMem[i].number < minFrameIndex) {
                minFrameIndex = ctx->pictureMem[i].number;
                index = i;
            }
        }
    }
    if ((*gopChangeCnt) > 0)
    {
        for (int i = 0; i < ctx->bufferCnt; i++) {
            if (ctx->pictureMem[i].used && ctx->pictureMem[i].gopChangeIdr && (uint32_t)ctx->pictureMem[i].number < minFrameIndex + 4 && !ctx->pictureMem[i].forceIDR) {
                ctx->pictureMem[i].gopChangeIdr = 0;
            }
        }
    }
    return index;
}

static int32_t getInputBuffer(struct video_encoder_private_context *ctx, int32_t number,
                              EncInputBuffer **inputBuffer)
{
    int32_t index = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && ctx->pictureMem[i].number == number) {
            *inputBuffer = &ctx->pictureMem[i];
            index = i;
            LOG_DEBUG(ENC,
                      "getInputBuffer: ctx->bufferCnt %d, index %d-%d, busAddr 0x%llx, number %d",
                      ctx->bufferCnt, index, ctx->pictureMem[i].index,
                      (U64)ctx->pictureMem[i].mem.busAddress, number);
            break;
        }
    }
    return index;
}

static int32_t getRemainInputBuffer(struct video_encoder_private_context *ctx,
                                    EncInputBuffer **inputBuffer)
{
    int32_t index = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used /* && !ctx->pictureMem[i].sent2Encoder*/) {
            *inputBuffer = &ctx->pictureMem[i];
            index = i;
            LOG_DEBUG(
                ENC,
                "getRemainInputBuffer: ctx->bufferCnt %d, index %d-%d, busAddr 0x%llx, NUMBER %d",
                ctx->bufferCnt, index, ctx->pictureMem[i].index, (U64)ctx->pictureMem[i].mem.busAddress,
                ctx->pictureMem[i].number);
            break;
        }
    }
    return index;
}

static int32_t getNotEncodedBufferCnt(struct video_encoder_private_context *ctx)
{
    int32_t cnt = 0;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && !ctx->pictureMem[i].sent2Encoder) {
            cnt++;
        }
    }
    return cnt;
}

// get index of input buffer that has not been encoded yet with minimum number?
static int32_t getMinNotEncodedBufferIndex(struct video_encoder_private_context *ctx)
{
    int32_t index = -1;
    uint32_t minFrameIndex = -1;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && !ctx->pictureMem[i].sent2Encoder) {

            if ((uint32_t)ctx->pictureMem[i].number < minFrameIndex) {
                minFrameIndex = ctx->pictureMem[i].number;
                index = i;
            }
        }
    }
    return index;
}

static int32_t getBufferCntNeed2Flush(struct video_encoder_private_context *ctx, int32_t number)
{
    int32_t cnt = 0;
    for (int i = 0; i < ctx->bufferCnt; i++) {
        if (ctx->pictureMem[i].used && ctx->pictureMem[i].number < number) {
            cnt++;
        }
    }
    return cnt;
}

static int getIdleROIMem(struct video_encoder_private_context *ctx, EWLLinearMem_t **deltaQp,
                         EWLLinearMem_t **cuCtrlInfo, EWLLinearMem_t **cuCtrlIndex)
{
    assert(deltaQp);
    int index = -1;

    // We bind deltaQ, cuCtrlInfo and cuCtrlIndex together,
    // so we could use one array to store all roi map relative buffer, like this:
    //  |<---------------------MAX_EWL_MEM_NUM--------------------->|
    //  |<--MAX_DELAY_NUM-->|<--MAX_DELAY_NUM-->|<--MAX_DELAY_NUM-->|
    //  |   roiMapDeltaQp   |  roimapCuCtrlInfo | roimapCuCtrlIndex |
    //  |1|2|...|-|-|.....|-|.................|-|.................|-|

    for (int i = 0; i < ctx->bufferCnt /*MAX_DELAY_NUM*/; i++) {
        if (!ctx->roiMemFactory[i].used) {
            LOG_DEBUG(ENC, "getIdleEWLMem: index %d, busAddr 0x%llx", i,
                      (U64)ctx->roiMemFactory[i].mem.busAddress);
            ctx->roiMemFactory[i].used = 1;
            index = i;
            *deltaQp = &ctx->roiMemFactory[i].mem;
            if (cuCtrlInfo)
                *cuCtrlInfo = &ctx->roiMemFactory[i + MAX_DELAY_NUM].mem;
            if (cuCtrlIndex)
                *cuCtrlIndex = &ctx->roiMemFactory[i + MAX_DELAY_NUM * 2].mem;
            break;
        }
    }
    return index;
}

static void setROIMemIdle(struct video_encoder_private_context *ctx, EWLLinearMem_t *mem)
{
    for (int i = 0; i < ctx->bufferCnt /*MAX_DELAY_NUM*/; i++) {
        if (mem == &ctx->roiMemFactory[i].mem) {
            ctx->roiMemFactory[i].used = 0;
        }
    }
}

static uint8_t *getIdleSEIBuffer(struct video_encoder_private_context *ctx, uint32_t size)
{
    uint8_t *buf = NULL;
    assert(size > 0);
    if (size == 0)
        return buf;
    /* priority: buffer with enough size >> realloc unused smaller buffer */
    for (int i = 0; i < MAX_SEI_BUFFER_NUM; i++) {
        if (!ctx->seiBuffer[i].used && ctx->seiBuffer[i].data && ctx->seiBuffer[i].size >= size) {
            ctx->seiBuffer[i].used = 1;
            buf = ctx->seiBuffer[i].data;
            //memset(ctx->seiBuffer[i].data, 0, ctx->seiBuffer[i].size);
            break;
        }
    }

    if (!buf) {
        for (int i = 0; i < MAX_SEI_BUFFER_NUM; i++) {
            if (!ctx->seiBuffer[i].used) {
                buf = (uint8_t *)realloc(ctx->seiBuffer[i].data, size);
                if (!buf) {
                    LOG_ERROR(ENC, "realloc memory failed: new size %d, old buffer[%d] %p %d", size,
                              i, ctx->seiBuffer[i].data, ctx->seiBuffer[i].size);
                    break;
                }
                ctx->seiBuffer[i].used = 1;
                ctx->seiBuffer[i].data = buf;
                ctx->seiBuffer[i].size = size;
                //memset(ctx->seiBuffer[i].data, 0, ctx->seiBuffer[i].size);
                break;
            }
        }
    }
    return buf;
}

static void setSEIBufferIdle(struct video_encoder_private_context *ctx, uint8_t *buffer)
{
    for (int i = 0; i < MAX_SEI_BUFFER_NUM; i++) {
        if (ctx->seiBuffer[i].used && ctx->seiBuffer[i].data == buffer) {
            ctx->seiBuffer[i].used = 0;
            break;
        }
    }
}

static void freeSEIBuffer(struct video_encoder_private_context *ctx)
{
    for (int i = 0; i < MAX_SEI_BUFFER_NUM; i++)
        if (ctx->seiBuffer[i].data)
            free(ctx->seiBuffer[i].data);
    memset(ctx->seiBuffer, 0, sizeof(ctx->seiBuffer));
}

static vmppResult getIdleAv1HeaderOutputBuffer(struct video_encoder_private_context *ctx, u8 **av1Header)
{
    int32_t idx = ctx->pictureEncCount % ctx->parallelCoreNum;
    if (ctx->av1Header[idx] == NULL)
    {
        ctx->av1Header[idx] = (uint8_t *)malloc(100);
        if (!(ctx->av1Header[idx])) {
            LOG_ERROR(ENC, "Fail to malloc private buffer.");
            return vmpp_RSLT_ERR_NO_BUFFER;
        }
    }
    *av1Header = ctx->av1Header[idx];
    return vmpp_RSLT_OK;
}

static u8 *getReadyAv1HeaderOutputBuffer(struct video_encoder_private_context *ctx)
{
    int32_t idx = 0;
    if (ctx->inputPictureCount < ctx->parallelCoreNum) {
        if (ctx->cfg.lookaheadDepth == 0)
        {
            idx = ctx->pictureEncCount - ctx->inputPictureCount - 1;
        } else {
            idx = ctx->pictureEncCount - ctx->inputPictureCount - ctx->frameDelay - 1;
        }
    } else {
        idx  = (ctx->pictureEncCount - 1 - (ctx->frameDelay - 1)) % ctx->parallelCoreNum;
    }

    return ctx->av1Header[idx];
}

static void freeAv1HeaderOutputBuffer (struct video_encoder_private_context *ctx)
{
    for (int i = 0; i < MAX_CORE_NUM; i++)
        if (ctx->av1Header[i])
            free(ctx->av1Header[i]);
    memset(ctx->av1Header, 0, sizeof(ctx->av1Header));
}

/*------------------------------------------------------------------------------
    The DMA engine is not part of this SDK version. The buffers of the encoder
    are allocated with EWLMallocLinear() and are mapped into the CPU address
    space, so the data is moved with memcpy()/memset() and its visibility for
    the hardware is controlled with EWLSyncMemData().
------------------------------------------------------------------------------*/
static void dmaMemZero(EWLLinearMem_t *mem, uint32_t size)
{
    if (mem != NULL && mem->virtualAddress != NULL)
        memset(mem->virtualAddress, 0, size);
}

/* Determines the coding type of the next picture from the GOP configuration,
   the library of this version does not provide VCEncFindNextPic(). */
static VCEncPictureCodingType findNextPictureType(VCEncIn *encIn, int32_t nextGopSize)
{
    VCEncGopConfig *gopCfg = &encIn->gopConfig;

    if (gopCfg->pGopPicCfg == NULL || gopCfg->size == 0 || nextGopSize <= 0)
        return VCENC_INTRA_FRAME;

    int32_t offset = (nextGopSize <= MAX_GOP_SIZE) ? gopCfg->gopCfgOffset[nextGopSize]
                                                   : gopCfg->gopCfgOffsetLP;
    int32_t idx = offset + (int32_t)(encIn->picture_cnt % (u32)nextGopSize);

    if (idx < 0 || idx >= gopCfg->size)
        idx = offset;
    if (idx < 0 || idx >= gopCfg->size)
        return VCENC_INTRA_FRAME;

    return gopCfg->pGopPicCfg[idx].codingType;
}

/* Returns the size of the additional CU information of one frame. */
static uint32_t getCuInfoTotalSize(struct video_encoder_private_context *ctx, uint32_t width,
                                   uint32_t height)
{
    u32 cuInfoSize = 0, cuInfoStride = 0, cuInfoTableSize = 0, aqInfoSize = 0, aqInfoStride = 0;
    i32 total = EncAsicGetCuInfoBufferSize(width, height, IS_H264(ctx->codecFormat) ? 16 : 64,
                                           (u32)(ctx->cfg.cuInfoVersion < 0 ? 1
                                                                           : ctx->cfg.cuInfoVersion),
                                           16, 16, &cuInfoSize, &cuInfoStride, &cuInfoTableSize,
                                           &aqInfoSize, &aqInfoStride);
    return total > 0 ? (uint32_t)total : 0;
}

static vmppResult allocRes(struct va_enc_channel *chn, const vmppFrame *frame,
                           EWLLinearMem_t *buf_out, EWLLinearMem_t *buf_in,
                           EncInputBuffer *inputBuffer, uint32_t timeout)
{
    uint32_t lumaSize = 0, chromaSize = 0;
    uint32_t pictureSize = 0;
    uint32_t streamSize = 0;
    uint32_t vRet;
    encVideoConfiguration *videoConfig;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;

    uint32_t inputAlignment = 1 << ctx->cfg.exp_of_input_alignment;
    const void *ewlInst = ctx->ewlInst;
    uint8_t *userBuf;
    uint64_t start, tick;
    ptr_t temp_addr = 0;
    uint32_t padding_size = 0;
    uint32_t pictureSize_def = 0;
    uint32_t orgStreamSize;
    uint32_t width = frame->width;
    i32 pgsize = getpagesize();

    videoConfig = &(chn->params.videoConfig);

    start = va_gettime_ns();

    if (videoConfig->alignmentEnable == 0) {
        width = frame->stride[0];
    }

    getAlignedPicSizebyFormat(picformatPar2Internal(frame->pixelFormat), width, /*frame->width*/
                              frame->height, inputAlignment, &lumaSize, &chromaSize, &pictureSize);

    if ((IS_HEVC(ctx->codecFormat) || IS_AV1(ctx->codecFormat)) && (videoConfig->lookaheadDepth != 0)) {
        padding_size = ((frame->width + 63) & (~63)) * 64;
        pictureSize_def = pictureSize;
        pictureSize = pictureSize + padding_size;
    }

    if (frame->memoryType == vmpp_MEM_HOST) {
        if (buf_in->size < pictureSize && buf_in->busAddress) {
            EWLFreeLinear(ewlInst, buf_in);
            buf_in->busAddress = (vmppDevAddr)NULL;
        }
        if (!buf_in->busAddress) {
            memset(buf_in, 0, sizeof(EWLLinearMem_t));
            buf_in->mem_type = EWL_MEM_TYPE_DPB;
            vRet = EWLMallocLinear(ewlInst, pictureSize + 2 * pgsize, inputAlignment, buf_in);
            if (vRet != EWL_OK) {
                LOG_ERROR(ENC, "Failed to allocate output buffer: ERR %d, SIZE %d", vRet,
                          pictureSize);
                buf_in->busAddress = (vmppDevAddr)NULL;
                return vmpp_RSLT_ERR_ENC_EWL;
            }
            if ((IS_HEVC(ctx->codecFormat) || IS_AV1(ctx->codecFormat))  && (videoConfig->lookaheadDepth != 0)) {
                temp_addr = ((ptr_t)buf_in->virtualAddress + pictureSize_def) & (~(ptr_t)0xfff);
                padding_size = (ptr_t)buf_in->virtualAddress + pictureSize - temp_addr;

                if (buf_in->virtualAddress != NULL)
                    memset((void *)temp_addr, 0, padding_size);
            }
#ifdef HEVC_2PASS_MISMATCH_WORKAROUND
            // vRet = EWLMallocLinear(ewlInst, pictureSize, inputAlignment, buf_in);
            // buf_in->virtualAddress = dmaMemGetSet(dieIndex, pictureSize, buf_in->busAddress);
#endif
        }
        /* Keep the mapping of the EWL input buffer returned by EWLMallocLinear().
         * The caller's picture is copied into it by HybridDMATransWrite()/copyInputPicture()
         * (both use mem.virtualAddress as destination) and the encoder reads it through
         * mem.busAddress (encIn->busLuma and the U/V planes follow mem.busAddress, so the
         * picture has to be stored in one contiguous internal buffer, also for the frames
         * that are kept as reference). Pointing virtualAddress at the caller's frame made
         * every copy write in front of the caller's allocation and left the input empty. */
    } else if (frame->memoryType == vmpp_MEM_DEVICE) {
        //LOG_WARN(ENC, "dev mem: width %d, height %d, stride %d, datasize %d", frame->width, frame->height, frame->stride[0], frame->dataSize);

        lumaSize = frame->stride[0] * frame->height;
        chromaSize = lumaSize / 2;

        buf_in->busAddress = buf_in->allocBusAddr = frame->busAddress[0];
        buf_in->size = frame->dataSize;
        buf_in->virtualAddress = buf_in->allocVirtualAddr = (u32 *)frame->busAddress[0];
    }

    inputBuffer->lumaSize = lumaSize;
    inputBuffer->chromaSize = chromaSize;

    streamSize = pictureSize * 2 < 1000000 ? 1000000 : pictureSize * 2;
    streamSize = CLIP3(VCENC_STREAM_MIN_BUF0_SIZE, VCENC_STREAM_MAX_BUF0_SIZE, streamSize);
    orgStreamSize = streamSize;

    if(IS_AV1(ctx->codecFormat))
    {
        // AV1 precarry buf is following to outputbuf
        streamSize = ((streamSize + (pgsize - 1)) & (~(pgsize - 1))) * 3;
    }

    if (buf_out->size < streamSize && buf_out->busAddress) {
        EWLFreeLinear(ewlInst, buf_out);
        buf_out->busAddress = (vmppDevAddr)NULL;
    }

    if (videoConfig->enableOutputCuInfo)
        userBuf = get_idle_out_buffer(chn, orgStreamSize + getCuInfoTotalSize(ctx, frame->width, frame->height));
    else
        userBuf = get_idle_out_buffer(chn, orgStreamSize);
    while (userBuf == NULL) {
        tick = va_gettime_ns();
        if ((float)(tick - start) / 1000000.0 > timeout) {
            LOG_WARN(ENC, "Timeout for Video encoder: No Output User Buffer");
            return vmpp_RSLT_ERR_NO_BUFFER;
        }
        sched_yield();
        if (videoConfig->enableOutputCuInfo)
            userBuf = get_idle_out_buffer(chn, orgStreamSize + getCuInfoTotalSize(ctx, frame->width, frame->height));
        else
            userBuf = get_idle_out_buffer(chn, orgStreamSize);
        if (userBuf) {
            LOG_WARN(ENC, "Video encoder wait Output User Buffer %lld ms", (U64)((tick - start) / 1000000));
        }
    }

    if (!buf_out->busAddress) {
        memset(buf_out, 0, sizeof(EWLLinearMem_t));
        buf_out->mem_type = EWL_MEM_TYPE_SLICE;
        vRet = EWLMallocLinear(ewlInst, streamSize, inputAlignment, buf_out);
        if (vRet != EWL_OK) {
            LOG_ERROR(ENC, "Failed to allocate output buffer: ERR %d, SIZE %d", vRet, streamSize);
            buf_out->busAddress = (vmppDevAddr)NULL;
            return vmpp_RSLT_ERR_ENC_EWL;
        }
#ifdef HEVC_2PASS_MISMATCH_WORKAROUND
        // vRet = EWLMallocLinear(ewlInst, pictureSize, inputAlignment, buf_in);
        // buf_out->virtualAddress = dmaMemGetSet(dieIndex, pictureSize, buf_out->busAddress);
#endif
    }

    buf_out->allocVirtualAddr = buf_out->virtualAddress = (u32 *)userBuf;
    inputBuffer->orgStreamSize = orgStreamSize;

    return vmpp_RSLT_OK;
}

static void freeEWLRes(struct video_encoder_private_context *ctx, const void *ewl_inst)
{
    if (ctx && ewl_inst) {
        for (int i = 0; i < MAX_DELAY_NUM; i++) {
            if (ctx->pictureMem[i].mem.busAddress != (vmppDevAddr)NULL &&
                ctx->pictureMem[i].memType == vmpp_MEM_HOST)
                EWLFreeLinear(ewl_inst, &ctx->pictureMem[i].mem);
        }

        for (int i = 0; i < MAX_CORE_NUM; i++) {
            if (ctx->outbufMemFactory[i].busAddress != (vmppDevAddr)NULL)
                EWLFreeLinear(ewl_inst, &ctx->outbufMemFactory[i]);
        }

        for (int i = 0; i < MAX_CORE_NUM; i++) {
            if (ctx->extSRAMMemFactory[i].busAddress != (vmppDevAddr)NULL)
                EWLFreeLinear(ewl_inst, &ctx->extSRAMMemFactory[i]);
        }

        for (int i = 0; i < MAX_EWL_MEM_NUM; i++) {
            if (ctx->roiMemFactory[i].mem.busAddress != (vmppDevAddr)NULL)
                EWLFreeLinear(ewl_inst, &ctx->roiMemFactory[i].mem);
        }

#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
        if (ctx->roiMapDeltaQpMemFactory[0].busAddress != (vmppDevAddr)NULL) {
            for (int coreIdx = 1; coreIdx < ctx->bufferCnt; coreIdx++)
                ctx->roiMapDeltaQpMemFactory[0].size += ctx->roiMapDeltaQpMemFactory[coreIdx].size;
            EWLFreeLinear(ewl_inst, &ctx->roiMapDeltaQpMemFactory[0]);
        }
#endif
    }
}

static void freeRes(struct va_enc_channel *chn)
{
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    const void *ewl_inst = NULL;
    if (!ctx)
        return;
    ewl_inst = ctx->ewlInst;

    for (int i = 0; i < MAX_DELAY_NUM; i++) {
        if (ctx->pictureMem[i].extSEI)
            free(ctx->pictureMem[i].extSEI);
    }
    freeEWLRes(ctx, ewl_inst);

    freeSEIBuffer(ctx);
}

static void HEVCSliceReady(VCEncSliceReady *slice) { UNUSED_PARAMETER(slice); }

static char *nextToken(char *str)
{
    char *p = strchr(str, ' ');
    if (p) {
        while (*p == ' ') p++;
        if (*p == '\0')
            p = NULL;
    }
    return p;
}

static int ParseGopConfigString(char *line, VCEncGopConfig *gopCfg, int frame_idx, int gopSize)
{
    if (!line)
        return -1;

    // format: FrameN Type POC QPoffset QPfactor  num_ref_pics ref_pics  used_by_cur
    int frameN, poc, num_ref_pics, i;
    char type[10];
    VCEncGopPicConfig *cfg = NULL;
    VCEncGopPicSpecialConfig *scfg = NULL;

    // frame idx
    sscanf(line, "Frame%d", &frameN);
    if ((frameN != (frame_idx + 1)) && (frameN != 0))
        return -1;

    if (frameN > gopSize)
        return 0;

    if (0 == frameN) {
        // format: FrameN Type  QPoffset  QPfactor   TemporalId  num_ref_pics   ref_pics used_by_cur
        // LTR    Offset   Interval
        scfg = &(gopCfg->pGopPicSpecialCfg[gopCfg->special_size++]);

        // frame type
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%s", type);
        scfg->nonReference = 0;
        if (strcmp(type, "I") == 0 || strcmp(type, "i") == 0)
            scfg->codingType = VCENC_INTRA_FRAME;
        else if (strcmp(type, "CRA") == 0 || strcmp(type, "cra") == 0)
            scfg->codingType = VCENC_CRA_FRAME;
        else if (strcmp(type, "P") == 0 || strcmp(type, "p") == 0)
            scfg->codingType = VCENC_PREDICTED_FRAME;
        else if (strcmp(type, "B") == 0 || strcmp(type, "b") == 0)
            scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
        /* P frame not for reference */
        else if (strcmp(type, "nrefP") == 0) {
            scfg->codingType = VCENC_PREDICTED_FRAME;
            scfg->nonReference = 1;
        }
        /* B frame not for reference */
        else if (strcmp(type, "nrefB") == 0) {
            scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
            scfg->nonReference = 1;
        } else
            scfg->codingType = scfg->nonReference = FRAME_TYPE_RESERVED;

        // qp offset
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &(scfg->QpOffset));

        // qp factor
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%lf", &(scfg->QpFactor));
        scfg->QpFactor = sqrt(scfg->QpFactor);

        // temporalId factor
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &(scfg->temporalId));

        // num_ref_pics
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &num_ref_pics);
        if (num_ref_pics > VCENC_MAX_REF_FRAMES) /* NUMREFPICS_RESERVED -1 */
        {
            LOG_ERROR(ENC, "GOP Config: Error, num_ref_pic can not be more than %d",
                      VCENC_MAX_REF_FRAMES);
            return -1;
        }
        scfg->numRefPics = num_ref_pics;

        if ((scfg->codingType == VCENC_INTRA_FRAME) && (0 == num_ref_pics))
            num_ref_pics = 1;
        // ref_pics
        for (i = 0; i < num_ref_pics; i++) {
            line = nextToken(line);
            if (!line)
                return -1;
            if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0)) {
                sscanf(line, "%c%d", &type[0], &(scfg->refPics[i].ref_pic));
                scfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(scfg->refPics[i].ref_pic - 1);
            } else {
                sscanf(line, "%d", &(scfg->refPics[i].ref_pic));
            }
        }
        if (i < num_ref_pics)
            return -1;

        // used_by_cur
        for (i = 0; i < num_ref_pics; i++) {
            line = nextToken(line);
            if (!line)
                return -1;
            sscanf(line, "%u", &(scfg->refPics[i].used_by_cur));
        }
        if (i < num_ref_pics)
            return -1;

        // LTR
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &scfg->i32Ltr);
        if (VCENC_MAX_LT_REF_FRAMES < scfg->i32Ltr)
            return -1;

        // Offset
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &scfg->i32Offset);

        // Interval
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &scfg->i32Interval);

        if (0 != scfg->i32Ltr) {
            gopCfg->u32LTR_idx[gopCfg->ltrcnt] = LONG_TERM_REF_ID2DELTAPOC(scfg->i32Ltr - 1);
            gopCfg->ltrcnt++;
            if (VCENC_MAX_LT_REF_FRAMES < gopCfg->ltrcnt)
                return -1;
        }

        // short_change
        scfg->i32short_change = 0;
        if (0 == scfg->i32Ltr) {
            /* not long-term ref */
            scfg->i32short_change = 1;
            for (i = 0; i < num_ref_pics; i++) {
                if (IS_LONG_TERM_REF_DELTAPOC(scfg->refPics[i].ref_pic) &&
                    (0 != scfg->refPics[i].used_by_cur)) {
                    scfg->i32short_change = 0;
                    break;
                }
            }
        }
    } else {
        // format: FrameN Type  POC  QPoffset    QPfactor   TemporalId  num_ref_pics  ref_pics
        // used_by_cur
        cfg = &(gopCfg->pGopPicCfg[gopCfg->size++]);

        // frame type
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%s", type);
        cfg->nonReference = 0;
        if (strcmp(type, "CRA") == 0 || strcmp(type, "cra") == 0)
            cfg->codingType = VCENC_CRA_FRAME;
        else if (strcmp(type, "P") == 0 || strcmp(type, "p") == 0)
            cfg->codingType = VCENC_PREDICTED_FRAME;
        else if (strcmp(type, "B") == 0 || strcmp(type, "b") == 0)
            cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
        /* P frame not for reference */
        else if (strcmp(type, "nrefP") == 0) {
            cfg->codingType = VCENC_PREDICTED_FRAME;
            cfg->nonReference = 1;
        }
        /* B frame not for reference */
        else if (strcmp(type, "nrefB") == 0) {
            cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
            cfg->nonReference = 1;
        } else
            return -1;

        // poc
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &poc);
        if (poc < 1 || poc > gopSize)
            return -1;
        cfg->poc = poc;

        // qp offset
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &(cfg->QpOffset));

        // qp factor
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%lf", &(cfg->QpFactor));
        // sqrt(QpFactor) is used in calculating lambda
        cfg->QpFactor = sqrt(cfg->QpFactor);

        // temporalId factor
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &(cfg->temporalId));

        // num_ref_pics
        line = nextToken(line);
        if (!line)
            return -1;
        sscanf(line, "%d", &num_ref_pics);
        if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES) {
            LOG_ERROR(ENC, "GOP Config: Error, num_ref_pic can not be more than %d",
                      VCENC_MAX_REF_FRAMES);
            return -1;
        }

        // ref_pics
        for (i = 0; i < num_ref_pics; i++) {
            line = nextToken(line);
            if (!line)
                return -1;
            if ((strncmp(line, "L", 1) == 0) || (strncmp(line, "l", 1) == 0)) {
                sscanf(line, "%c%d", &type[0], &(cfg->refPics[i].ref_pic));
                cfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(cfg->refPics[i].ref_pic - 1);
            } else {
                sscanf(line, "%d", &(cfg->refPics[i].ref_pic));
            }
        }
        if (i < num_ref_pics)
            return -1;

        // used_by_cur
        for (i = 0; i < num_ref_pics; i++) {
            line = nextToken(line);
            if (!line)
                return -1;
            sscanf(line, "%u", &(cfg->refPics[i].used_by_cur));
        }
        if (i < num_ref_pics)
            return -1;

        cfg->numRefPics = num_ref_pics;
    }

    return 0;
}

static int ReadGopConfig(/*char *fname, */ char **config, VCEncGopConfig *gopCfg, int gopSize,
                         uint8_t *gopCfgOffset)
{
    int ret = -1;

    if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
        return -1;

    if (gopCfgOffset)
        gopCfgOffset[gopSize] = gopCfg->size;
    if (config) {
        int id = 0;
        while (config[id]) {
            ParseGopConfigString(config[id], gopCfg, id, gopSize);
            id++;
        }
        ret = 0;
    }
    return ret;
}

static vmppResult InitGopConfigs(struct video_encoder_private_context *ctx,
                                 struct va_enc_channel *chn, int pass2)
{
    int gopSize = chn->params.videoConfig.gopSize;
    int preLoadNum = 0;
    VCEncGopConfig *gopCfg = &ctx->encIn->gopConfig;
    int interlacedFrame = 0; // TODO
    CLIENT_TYPE client_type =
        IS_H264(ctx->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC : EWL_CLIENT_TYPE_HEVC_ENC;
    VCEncBuild ver = VCEncGetBuild(client_type);
    u32 workmode = ctx->workmode;

    char **rpsDefaultGop1 = RpsDefault_GOPSize_1;
    if (IS_H264(ctx->codecFormat))
        rpsDefaultGop1 = workmode == SINGLE_CORE_MODE ? RpsDefault_H264_GOPSize_1 : RpsDefault_H264_multicore_GOPSize_1;
    else if (HW_ID_MAJOR_NUMBER(ver.hwBuild) == 0x60)
        rpsDefaultGop1 = RpsDefault_V60_GOPSize_1;
    char **default_configs[16] = {
        ctx->gopLowdelay ? RpsLowdelayDefault_GOPSize_1 : rpsDefaultGop1,
        ctx->gopLowdelay ? RpsLowdelayDefault_GOPSize_2 : RpsDefault_GOPSize_2,
        ctx->gopLowdelay ? RpsLowdelayDefault_GOPSize_3 : RpsDefault_GOPSize_3,
        ctx->gopLowdelay ? RpsLowdelayDefault_GOPSize_4 : (workmode == SINGLE_CORE_MODE && IS_H264(ctx->codecFormat) ) ? RpsDefault_H264_GOPSize_4 : RpsDefault_GOPSize_4,
        RpsDefault_GOPSize_5,
        RpsDefault_GOPSize_6,
        RpsDefault_GOPSize_7,
        (workmode == SINGLE_CORE_MODE && IS_H264(ctx->codecFormat) ) ? RpsDefault_H264_GOPSize_8 : RpsDefault_GOPSize_8,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        RpsDefault_GOPSize_16};

#if 0 //test for LTR
    if (ctx->longterm_enable) {
        // long term only support gopSize = 4 and lookahead = 0 temporarily
         gopSize = chn->params.videoConfig.gopSize = 4;
        default_configs[3] = Rps_LongTerm_Int24;
    }
#endif

    if (gopSize < 0 || gopSize > MAX_GOP_SIZE ||
        (gopSize > 0 && default_configs[gopSize - 1] == NULL)) {
        LOG_ERROR(ENC, "GOP Config: Error, Invalid GOP Size: %d", gopSize);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (gopSize > 8 && chn->params.videoConfig.lookaheadDepth > 0) {
        LOG_ERROR(ENC, "GOP Config: Error, Invalid GOP Size for 2pass: %d", gopSize);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (chn->params.videoConfig.P2B == VMPP_ENC_DEFAULT_PAR) {
        chn->params.videoConfig.P2B = IS_HEVC(ctx->codecFormat) || IS_AV1(ctx->codecFormat);
    }

    if (IS_H264(ctx->codecFormat) && chn->params.videoConfig.profile == vmpp_VIDEO_PRFL_H264_BASELINE) {
        chn->params.videoConfig.P2B = 0;
    }

    if ((pass2 || (pass2 == 0 && gopSize == 1)) && chn->params.videoConfig.P2B) {
        default_configs[0] = RpsPass2_GOPSize_1;
        default_configs[1] = RpsPass2_GOPSize_2;
        default_configs[3] = RpsPass2_GOPSize_4;
        default_configs[7] = RpsPass2_GOPSize_8;
    }

    if (!chn->params.videoConfig.bBPyramid) {
        chn->params.videoConfig.P2B = 0;
        default_configs[0] = RpsDefault_GOPSize_1;
        default_configs[1] = RpsDefault_GOPSize_2;
        default_configs[2] = NonRefB_GOPSize_3;
        default_configs[3] = NonRefB_GOPSize_4;
        default_configs[4] = NonRefB_GOPSize_5;
        default_configs[5] = NonRefB_GOPSize_6;
        default_configs[6] = NonRefB_GOPSize_7;
        default_configs[7] = NonRefB_GOPSize_8;
    }

    if (chn->params.videoConfig.svcTLayers) {
        u32 svcMaxTLayer = chn->params.videoConfig.svcTLayers - 1;
        if (svcMaxTLayer > 3) {
            LOG_ERROR(ENC, "GOP Config: Error, Invalid SVC Max Temporal Layer: %d", svcMaxTLayer);
            return vmpp_RSLT_ERR_INVALID_PARAMS;
        }
        if (chn->params.videoConfig.P2B) {
            default_configs[0] = RpsPass2_GOPSize_1;
            default_configs[1] = RpsDefault_SVC_P2B_GOPSize_2;
            default_configs[3] = RpsDefault_SVC_P2B_GOPSize_4;
            default_configs[7] = RpsDefault_SVC_P2B_GOPSize_8;
        } else {
            default_configs[0] = rpsDefaultGop1;
            default_configs[1] = RpsDefault_SVC_GOPSize_2;
            default_configs[3] = RpsDefault_SVC_GOPSize_4;
            default_configs[7] = RpsDefault_SVC_GOPSize_8;
        }
        int gopSizeSVC[4] = {1, 2, 4, 8};
        gopSize = chn->params.videoConfig.gopSize = gopSizeSVC[svcMaxTLayer];
    }

    // Handle Interlace
    if (interlacedFrame && gopSize == 1)
        default_configs[0] = RpsDefault_Interlace_GOPSize_1;

    if (gopSize > 8)
        preLoadNum = 4;
    else if (gopSize >= 4 || gopSize == 0)
        preLoadNum = 4;
    else
        preLoadNum = gopSize;

    gopCfg->special_size = 0;
    gopCfg->ltrcnt = 0;

    for (int i = 1; i <= preLoadNum; i++) {
        if (ReadGopConfig(default_configs[i - 1], gopCfg, i, gopCfg->gopCfgOffset))
            return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (gopSize == 0) {
        // gop6
        if (ReadGopConfig(default_configs[5], gopCfg, 6, gopCfg->gopCfgOffset))
            return vmpp_RSLT_ERR_ENC_INIT_GOP;
        // gop8
        if (ReadGopConfig(default_configs[7], gopCfg, 8, gopCfg->gopCfgOffset))
            return vmpp_RSLT_ERR_ENC_INIT_GOP;
    } else if (gopSize > 4) {
        if (ReadGopConfig(default_configs[gopSize - 1], gopCfg, gopSize, gopCfg->gopCfgOffset))
            return vmpp_RSLT_ERR_ENC_INIT_GOP;
    }

    if ((0 != chn->params.videoConfig.ltrInterval) && (gopCfg->special_size == 0))
    {

        if (gopSize != 1)
        {
            LOG_ERROR(ENC,"GOP Config: Error, when using --LTR configure option, the gopsize also should be set to 1!\n");
            return -1;
        }

        if (pass2)
        {
            LOG_ERROR(ENC,"GOP Config: Error, when using --LTR configure option, the lookaheadDepth also should be set to 0!\n");
            return -1;
        }

        if(chn->params.videoConfig.P2B)
        {
            LOG_ERROR(ENC,"GOP Config: Error, when using --LTR configure option, the P2B should be set to 0!\n");
            return -1;
        }
            
        gopCfg->pGopPicSpecialCfg[0].poc = 0;
        gopCfg->pGopPicSpecialCfg[0].QpOffset = chn->params.videoConfig.ltrQpDelta;
        gopCfg->pGopPicSpecialCfg[0].QpFactor = QPFACTOR_RESERVED;
        gopCfg->pGopPicSpecialCfg[0].temporalId = TEMPORALID_RESERVED;
        gopCfg->pGopPicSpecialCfg[0].codingType = FRAME_TYPE_RESERVED;
        gopCfg->pGopPicSpecialCfg[0].numRefPics = NUMREFPICS_RESERVED;
        gopCfg->pGopPicSpecialCfg[0].i32Ltr = 1;
        gopCfg->pGopPicSpecialCfg[0].i32Offset = 0;
        gopCfg->pGopPicSpecialCfg[0].i32Interval = chn->params.videoConfig.ltrInterval;
        gopCfg->pGopPicSpecialCfg[0].i32short_change = 0;
        gopCfg->u32LTR_idx[0]                    = LONG_TERM_REF_ID2DELTAPOC(0);
    
        gopCfg->pGopPicSpecialCfg[1].poc = 0;
        gopCfg->pGopPicSpecialCfg[1].QpOffset = QPOFFSET_RESERVED;
        gopCfg->pGopPicSpecialCfg[1].QpFactor = QPFACTOR_RESERVED;
        gopCfg->pGopPicSpecialCfg[1].temporalId = TEMPORALID_RESERVED;
        gopCfg->pGopPicSpecialCfg[1].codingType = FRAME_TYPE_RESERVED;
        gopCfg->pGopPicSpecialCfg[1].numRefPics = 1;
        gopCfg->pGopPicSpecialCfg[1].refPics[0].ref_pic     = LONG_TERM_REF_ID2DELTAPOC(0);
        gopCfg->pGopPicSpecialCfg[1].refPics[0].used_by_cur = 1;
        gopCfg->pGopPicSpecialCfg[1].refPics[1].ref_pic     = 0;
        gopCfg->pGopPicSpecialCfg[1].refPics[1].used_by_cur = 0;
        gopCfg->pGopPicSpecialCfg[1].i32Ltr = 0;
        gopCfg->pGopPicSpecialCfg[1].i32Offset = 0;
        gopCfg->pGopPicSpecialCfg[1].i32Interval = chn->params.videoConfig.ltrRefGap;
        gopCfg->pGopPicSpecialCfg[1].i32short_change = 0;
    
        gopCfg->special_size = 2;
        gopCfg->ltrcnt = 1;
    }

    // lowDelay auto detection
    VCEncGopPicConfig *cfgStart = &(gopCfg->pGopPicCfg[gopCfg->gopCfgOffset[gopSize]]);
    if (gopSize == 1) {
        ctx->gopLowdelay = 1;
    } else if ((gopSize > 1) && (ctx->gopLowdelay == 0)) {
        ctx->gopLowdelay = 1;
        for (int i = 1; i < gopSize; i++) {
            if (cfgStart[i].poc < cfgStart[i - 1].poc) {
                ctx->gopLowdelay = 0;
                break;
            }
        }
    }
    // For lowDelay, Handle the first few frames that miss reference frame
    if (1) {
        int nGop;
        int idx = 0;
        int maxErrFrame = 0;
        VCEncGopPicConfig *cfg;

        // Find the max frame number that will miss its reference frame defined in rps
        while ((idx - maxErrFrame) < gopSize) {
            nGop = (idx / gopSize) * gopSize;
            cfg = &(cfgStart[idx % gopSize]);

            for (uint32_t i = 0; i < cfg->numRefPics; i++) {
                // POC of this reference frame
                int refPoc = cfg->refPics[i].ref_pic + cfg->poc + nGop;
                if (refPoc < 0) {
                    maxErrFrame = idx + 1;
                }
            }
            idx++;
        }

        // Try to config a new rps for each "error" frame by modifying its original rps
        for (idx = 0; idx < maxErrFrame; idx++) {
            int j, iRef, nRefsUsedByCur, nPoc;
            VCEncGopPicConfig *cfgCopy;

            if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
                break;

            // Add to array end
            cfg = &(gopCfg->pGopPicCfg[gopCfg->size]);
            cfgCopy = &(cfgStart[idx % gopSize]);
            memcpy(cfg, cfgCopy, sizeof(VCEncGopPicConfig));
            gopCfg->size++;

            // Copy reference pictures
            nRefsUsedByCur = iRef = 0;
            nPoc = cfgCopy->poc + ((idx / gopSize) * gopSize);
            for (uint32_t i = 0; i < cfgCopy->numRefPics; i++) {
                int newRef = 1;
                int used_by_cur = cfgCopy->refPics[i].used_by_cur;
                int ref_pic = cfgCopy->refPics[i].ref_pic;
                // Clip the reference POC
                if ((cfgCopy->refPics[i].ref_pic + nPoc) < 0)
                    ref_pic = 0 - (nPoc);

                // Check if already have this reference
                for (j = 0; j < iRef; j++) {
                    if (cfg->refPics[j].ref_pic == ref_pic) {
                        newRef = 0;
                        if (used_by_cur)
                            cfg->refPics[j].used_by_cur = used_by_cur;
                        break;
                    }
                }

                // Copy this reference
                if (newRef) {
                    cfg->refPics[iRef].ref_pic = ref_pic;
                    cfg->refPics[iRef].used_by_cur = used_by_cur;
                    iRef++;
                }
            }
            cfg->numRefPics = iRef;
            // If only one reference frame, set P type.
            for (uint32_t i = 0; i < cfg->numRefPics; i++) {
                if (cfg->refPics[i].used_by_cur)
                    nRefsUsedByCur++;
            }
            if (nRefsUsedByCur == 1)
                cfg->codingType = VCENC_PREDICTED_FRAME;
        }
    }

    return vmpp_RSLT_OK;
}

static void InitPicConfig(VCEncIn *pEncIn, struct va_enc_channel *chn)
{
    int32_t i, j, k, poc;
    int32_t maxpicOrderCntLsb = 1 << 16;

    ASSERT(pEncIn != NULL);

    pEncIn->gopCurrPicConfig.codingType = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.nonReference = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.numRefPics = NUMREFPICS_RESERVED;
    pEncIn->gopCurrPicConfig.poc = -1;
    pEncIn->gopCurrPicConfig.QpFactor = QPFACTOR_RESERVED;
    pEncIn->gopCurrPicConfig.QpOffset = QPOFFSET_RESERVED;
    pEncIn->gopCurrPicConfig.temporalId = 0;
    pEncIn->i8SpecialRpsIdx = -1;
    for (k = 0; k < VCENC_MAX_REF_FRAMES; k++) {
        pEncIn->gopCurrPicConfig.refPics[k].ref_pic = INVALITED_POC;
        pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = 0;
    }

    for (k = 0; k < VCENC_MAX_LT_REF_FRAMES; k++) pEncIn->long_term_ref_pic[k] = INVALITED_POC;

    pEncIn->bIsPeriodUsingLTR = HANTRO_FALSE;
    pEncIn->bIsPeriodUpdateLTR = HANTRO_FALSE;

    for (i = 0; i < pEncIn->gopConfig.special_size; i++) {
        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Interval <= 0)
            continue;

        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr == 0)
            pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
        else {
            pEncIn->bIsPeriodUpdateLTR = HANTRO_TRUE;

            for (k = 0; k < (int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[i].numRefPics; k++) {
                int32_t i32LTRIdx = pEncIn->gopConfig.pGopPicSpecialCfg[i].refPics[k].ref_pic;

                if ((IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) &&
                    ((pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr - 1) ==
                     LONG_TERM_REF_DELTAPOC2ID(i32LTRIdx))) {
                    pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
                }
            }
        }
    }

    memset(pEncIn->bLTR_need_update, 0, sizeof(u32) * VCENC_MAX_LT_REF_FRAMES);
    pEncIn->bIsIDR = HANTRO_TRUE;

    poc = 0;
    /* check current picture encoded as LTR*/
    pEncIn->u8IdxEncodedAsLTR = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
        if (pEncIn->bIsPeriodUsingLTR == HANTRO_FALSE)
            break;

        /* A long term reference is updated either periodically (i32Interval > 0)
           or exactly one time (i32Ltr > 0, i32Interval == 0, unused so far). */
        int32_t bLTRUpdatePeriod = (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval > 0);
        int32_t bLTRUpdateOneTimes =
            (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr > 0) &&
            (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0) &&
            (pEncIn->long_term_ref_pic[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1] ==
             INVALITED_POC);

        if (!(bLTRUpdatePeriod || bLTRUpdateOneTimes) ||
            (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
            continue;

        poc = poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

        if (poc < 0) {
            poc += maxpicOrderCntLsb;
            if (poc > (maxpicOrderCntLsb >> 1))
                poc = -1;
        }

        int32_t interval = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval
                               ? pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval
                               : maxpicOrderCntLsb;

        if ((poc >= 0) && (poc % interval == 0)) {
            /* more than one LTR at the same frame position */
            if (0 != pEncIn->u8IdxEncodedAsLTR) {
                // reuse the same POC LTR
                pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1] =
                    HANTRO_TRUE;
                continue;
            }

            pEncIn->gopCurrPicConfig.codingType =
                ((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED)
                    ? pEncIn->gopCurrPicConfig.codingType
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
            pEncIn->gopCurrPicConfig.nonReference =
                ((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference ==
                 FRAME_TYPE_RESERVED)
                    ? pEncIn->gopCurrPicConfig.nonReference
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference;
            pEncIn->gopCurrPicConfig.numRefPics =
                ((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED)
                    ? pEncIn->gopCurrPicConfig.numRefPics
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
            pEncIn->gopCurrPicConfig.QpFactor =
                (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED)
                    ? pEncIn->gopCurrPicConfig.QpFactor
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
            pEncIn->gopCurrPicConfig.QpOffset =
                (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
                    ? pEncIn->gopCurrPicConfig.QpOffset
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
            pEncIn->gopCurrPicConfig.temporalId =
                (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED)
                    ? pEncIn->gopCurrPicConfig.temporalId
                    : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

            if (((int32_t)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics !=
                 NUMREFPICS_RESERVED)) {
                for (k = 0; k < (int32_t)pEncIn->gopCurrPicConfig.numRefPics; k++) {
                    pEncIn->gopCurrPicConfig.refPics[k].ref_pic =
                        pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
                    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur =
                        pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
                }
            }

            pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
            pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
        }
    }
    uint32_t gopSize = chn->params.videoConfig.gopSize;
    bool adaptiveGop = (gopSize == 0);

    pEncIn->timeIncrement = 0;
    pEncIn->vui_timing_info_enable = 1;
    pEncIn->hashType = 0;
    pEncIn->poc = 0;
    // default gop size as IPPP
    pEncIn->gopSize = (adaptiveGop ? (chn->params.videoConfig.lookaheadDepth ? 4 : 1) : gopSize);
    pEncIn->last_idr_picture_cnt = pEncIn->picture_cnt = pEncIn->picture_gopIdx = 0;

    /* The fields isLatency and brcGopSize are not provided by this version. */
    UNUSED_PARAMETER(gopSize);
}

static int32_t AdaptiveGopDecision(vmppFrame *frame, VCEncIn *pEncIn, VCEncOut *pEncOut, u32 maxBFrames,
                                   int32_t *pNextGopSize, adapGopCtr *agop)
{
    int32_t nextGopSize = -1;
    float bpp;
    unsigned int uiIntraCu8Num = pEncOut->cuStatis.intraCu8Num;
    unsigned int uiSkipCu8Num = pEncOut->cuStatis.skipCu8Num;
    unsigned int uiPBFrameCost = pEncOut->cuStatis.PBFrame4NRdCost;
    double dIntraVsInterskip =
        (double)uiIntraCu8Num / (double)((frame->width / 8) * (frame->height / 8));
    double dSkipVsInterskip =
        (double)uiSkipCu8Num / (double)((frame->width / 8) * (frame->height / 8));

    agop->gop_frm_num++;
    agop->sum_intra_vs_interskip += dIntraVsInterskip;
    agop->sum_skip_vs_interskip += dSkipVsInterskip;
    agop->sum_costP += (pEncIn->codingType == VCENC_PREDICTED_FRAME) ? uiPBFrameCost : 0;
    agop->sum_costB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME) ? uiPBFrameCost : 0;
    agop->sum_intra_vs_interskipP +=
        (pEncIn->codingType == VCENC_PREDICTED_FRAME) ? dIntraVsInterskip : 0;
    agop->sum_intra_vs_interskipB +=
        (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME) ? dIntraVsInterskip : 0;

    agop->frmSize += pEncOut->streamSize;
    if (pEncIn->gopPicIdx ==
        pEncIn->gopSize - 1) // last frame of the current gop. decide the gopsize of next gop.
    {
        agop->frmSize = agop->frmSize / agop->gop_frm_num;
        bpp = (float)agop->frmSize * 1000 / frame->width / frame->height;
        dIntraVsInterskip = agop->sum_intra_vs_interskip / agop->gop_frm_num;
        dSkipVsInterskip = agop->sum_skip_vs_interskip / agop->gop_frm_num;
        agop->sum_costB =
            (agop->gop_frm_num > 1) ? (agop->sum_costB / (agop->gop_frm_num - 1)) : 0xFFFFFFF;
        agop->sum_intra_vs_interskipB =
            (agop->gop_frm_num > 1) ? (agop->sum_intra_vs_interskipB / (agop->gop_frm_num - 1))
                                    : 0xFFFFFFF;
        // Enabled adaptive GOP size for large resolution
        if ((frame->width * frame->height) >= (1280 * 720)) {
            nextGopSize = agop->last_gopsize;
            if ( ((((double)agop->sum_costP/(double)agop->sum_costB)<1.1)&&(dSkipVsInterskip >= 0.9)) || dIntraVsInterskip >= 0.30 || agop->sum_intra_vs_interskipP > 0.5 || (agop->sum_costP < agop->sum_costB && dIntraVsInterskip >= 0.2 && (dSkipVsInterskip + dIntraVsInterskip) >= 0.6))
	        {
              if (bpp >= 2.5)
                agop->last_gopsize = nextGopSize = 1;
              else
                agop->last_gopsize = nextGopSize = 3;
            }
	        else if (dIntraVsInterskip < 0.01 && dSkipVsInterskip > 0.99)
	        {
	            agop->last_gopsize = nextGopSize = 8;
	        }
	        else if (((double)agop->sum_costP / (double)agop->sum_costB) > 5) {
                nextGopSize = agop->last_gopsize;
            }
            else {
	            if (dIntraVsInterskip >= 0.20) {
                  agop->last_gopsize = nextGopSize = 2; //One B
                }
	        else if (dIntraVsInterskip >= 0.10) {
                agop->last_gopsize--;
                if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
                    agop->last_gopsize--;
                }
                agop->last_gopsize = MAX(agop->last_gopsize, 3);
                nextGopSize = agop->last_gopsize; //
            }
            else {
                agop->last_gopsize++;
                if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
                    agop->last_gopsize++;
                }
                agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                nextGopSize = agop->last_gopsize; //
                }
            }
        }
        else if ((MAX_ADAPTIVE_GOP_SIZE > 3) && ((frame->width * frame->height) >= (416 * 240)))
        {
            if ((((double)agop->sum_costP / (double)agop->sum_costB) < 1.1) && (dSkipVsInterskip >= 0.95)) {
                agop->last_gopsize = nextGopSize = 1;
            } else if (((double)agop->sum_costP / (double)agop->sum_costB) > 5) {
                nextGopSize = agop->last_gopsize;
            } else {
                if (((agop->sum_intra_vs_interskipP > 0.40) &&
                     (agop->sum_intra_vs_interskipP < 0.70) &&
                     (agop->sum_intra_vs_interskipB < 0.10))) {
                    agop->last_gopsize++;
                    if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                } else if (dIntraVsInterskip >= 0.30) {
                    agop->last_gopsize = nextGopSize = 1; // No B
                } else if (dIntraVsInterskip >= 0.20) {
                    agop->last_gopsize = nextGopSize = 2; // One B
                } else if (dIntraVsInterskip >= 0.10) {
                    agop->last_gopsize--;
                    if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
                        agop->last_gopsize--;
                    }
                    agop->last_gopsize = MAX(agop->last_gopsize, 3);
                    nextGopSize = agop->last_gopsize; //
                } else {
                    agop->last_gopsize++;
                    if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                }
            }
        } else {
            nextGopSize = 3;
        }
        agop->gop_frm_num = 0;
        agop->sum_intra_vs_interskip = 0;
        agop->sum_skip_vs_interskip = 0;
        agop->sum_costP = 0;
        agop->sum_costB = 0;
        agop->frmSize = 0;
        agop->sum_intra_vs_interskipP = 0;
        agop->sum_intra_vs_interskipB = 0;

        nextGopSize = MIN(nextGopSize, MAX_ADAPTIVE_GOP_SIZE);
    }

    if(maxBFrames < 7 && nextGopSize > (int32_t) maxBFrames + 1)
    {
        nextGopSize = maxBFrames + 1;

        if (nextGopSize == 5 || nextGopSize == 7)
        {
            agop->last_gopsize--;
            nextGopSize--;
        }
    }

    if (nextGopSize != -1)
        *pNextGopSize = nextGopSize;

    return nextGopSize;
}

static int32_t getNextGopSize(vmppFrame *frame, VCEncIn *pEncIn, VCEncInst encoder, u32 maxBFrames,
                              int32_t *pNextGopSize, adapGopCtr *agop, uint32_t lookaheadDepth,
                              VCEncOut *pEncOut)
{
    UNUSED_PARAMETER(encoder);
    if (lookaheadDepth) {
        /* The GOP size of pass 1 is not provided by this library version. */
        int32_t updGop = 0;
        if (updGop)
            *pNextGopSize = updGop;
    } else if (pEncIn->codingType != VCENC_INTRA_FRAME)
        AdaptiveGopDecision(frame, pEncIn, pEncOut, maxBFrames, pNextGopSize, agop);

    return *pNextGopSize;
}

static int32_t getNextGopSizeSingleThread(vmppFrame *frame, VCEncIn *pEncIn, VCEncInst encoder, u32 maxBFrames,
                              int32_t *pNextGopSize, adapGopCtr *agop, uint32_t lookaheadDepth,
                              VCEncOut *pEncOut)
{
    UNUSED_PARAMETER(encoder);
    if (lookaheadDepth) {
        /* The GOP size of pass 1 is not provided by this library version. */
        int32_t updGop = 0;
        if (updGop)
            *pNextGopSize = updGop;
    } else if (pEncIn->codingType != VCENC_INTRA_FRAME)
        AdaptiveGopDecision(frame, pEncIn, pEncOut, maxBFrames, pNextGopSize, agop);

    return *pNextGopSize;
}

static int32_t CheckArea(VCEncPictureArea *area, const vmppFrame *frame,
                         VCEncVideoCodecFormat codecType)
{
    int32_t max_cu_size = 64;
    if (IS_H264(codecType))
        max_cu_size = 16;

    int32_t w = (frame->width + max_cu_size - 1) / max_cu_size;
    int32_t h = (frame->height + max_cu_size - 1) / max_cu_size;

    if ((area->left < (uint32_t)w) && (area->right < (uint32_t)w) && (area->top < (uint32_t)h) &&
        (area->bottom < (uint32_t)h))
        return 1;

    return 0;
}

static void VCEncInputLineBufDone(void *pAppData) { UNUSED_PARAMETER(pAppData); }

static vmppResult checkParameters(struct video_encoder_private_context *ctx,
                                  struct va_enc_channel *chn, encChannelParameters *param)
{
    UNUSED_PARAMETER(chn);
    // check parameters
    //uint32_t client_type;
    //client_type = IS_H264(ctx->codecFormat) ? VCENC_VIDEO_CODEC_H264 : IS_HEVC(ctx->codecFormat) ? VCENC_VIDEO_CODEC_HEVC : VCENC_VIDEO_CODEC_AV1;
    const EWLHwConfig_t *asic_cfg = EncGetAsicConfig(ctx->codecFormat, ctx->ewlInst);
    ctx->roiMapVersion = asic_cfg != NULL ? asic_cfg->roiMapVersion : 0;
    encVideoConfiguration *video_par = &param->videoConfig;

    /* Encoded image width limits.*/
    if (video_par->width < VCENC_MIN_ENC_WIDTH ||
      video_par->width > VCENC_MAX_ENC_WIDTH || (video_par->width & 0x1) != 0) {
        LOG_ERROR(ENC,
                  "It does not support width %d! supported w(%d ~ %d) and must be even.\n",
                  video_par->width, VCENC_MIN_ENC_WIDTH, VCENC_MAX_ENC_WIDTH);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    /* Encoded image height limits.*/
    if (video_par->height < VCENC_MIN_ENC_HEIGHT ||
        video_par->height > VCENC_MAX_ENC_HEIGHT || (video_par->height & 0x1) != 0) {
        LOG_ERROR(ENC,
                  "It does not support height %d! supported h(%d ~ %d) and must be even.\n",
                  video_par->height, VCENC_MIN_ENC_HEIGHT, VCENC_MAX_ENC_HEIGHT);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->llRc > 5) {
        LOG_WARN(ENC, "Invalid lowLatencyRc value (%d)! Valid range: [0, 5], use 5 instead.",
                 video_par->llRc);
        video_par->llRc = 5;
    }

    // lookahead only support 0 4-40
    if (video_par->lookaheadDepth > 40 ||
        (video_par->lookaheadDepth < 4 && video_par->lookaheadDepth > 0)) {
        LOG_ERROR(ENC, "Invalid vast param lookaheadLength!");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->lookaheadDepth > 0 &&
        ((!asic_cfg->bFrameEnabled && video_par->gopSize != 1) || asic_cfg->cuInforVersion < 1)) {
        // lookahead needs bFrame support & cuInfo version 1
        video_par->lookaheadDepth = 0;
    }
    if (video_par->lookaheadDepth > 0 && asic_cfg->cuInforVersion != 2 &&
        asic_cfg->bMultiPassSupport) {
        LOG_ERROR(ENC, "IM only support cuInfo version 2!");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }
    if (video_par->lookaheadDepth) {
        ctx->gopLowdelay = 0; /* lookahead not work well with lowdelay configurations */
        if (!ctx->roiMapDeltaQpEnable) {
            ctx->roiMapDeltaQpEnable = 1;
            ctx->roiMapDeltaQpBlockUnit = 1;
            ctx->roiMapDeltaQpBlockUnit = IS_AV1(ctx->codecFormat)? 0 : MAX(1, video_par->roiMapDeltaQpBlockUnit);
        }
    }

    if (video_par->maxBFrames < 7) {
        if (video_par->gopSize != 0) {
            LOG_ERROR(ENC, "param maxBFrames can only be set when miniGopSize is 0.\n");
            return vmpp_RSLT_ERR_UNSUPPORTED;
        }
        video_par->gopSize = video_par->maxBFrames + 1;
    }
    if (video_par->gopSize > MAX_GOP_SIZE) {
        LOG_ERROR(ENC,
                  "It does not support this gop size %d! supported 0 ~ 16.\n",
                  video_par->gopSize);
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->gdrDuration > 0 && video_par->keyInt == 1) {
        LOG_WARN(ENC, "Invalid keyInt (%d) to support GDR (gdrDuration %d)! Disable GDR!",
                 video_par->keyInt, video_par->gdrDuration);
        video_par->gdrDuration = 0;
    }

    if (video_par->gdrDuration > 0 && video_par->gdrDuration > video_par->keyInt) {
        LOG_WARN(ENC, "gdrDuration (%d) should not larger than keyInt (%d)!",
                 video_par->gdrDuration, video_par->keyInt);
        video_par->gdrDuration = video_par->keyInt;
    }

    if (video_par->gdrDuration > 0 && (video_par->gopSize != 1 || video_par->lookaheadDepth > 0) && video_par->svcTLayers != 1) {
        LOG_WARN(ENC,
                 "gdrDuration (%d) only works with 1 pass and IPPP gop structure! "
                 "video_par->gopSize %d, video_par->lookaheadDepth %d",
                 video_par->gdrDuration, video_par->gopSize, video_par->lookaheadDepth);
        video_par->gdrDuration = 0;
    }

    if (video_par->roiType == vmpp_ENC_ROI_RANGE && video_par->lookaheadDepth > 0) {
        LOG_ERROR(ENC, "NOT support ROI range and 2-pass simultaneously");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->svcTLayers > 0 && video_par->lookaheadDepth > 0) {
        LOG_ERROR(ENC, "NOT support svc temporal layers and 2-pass simultaneously.");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->sliceSize > 0 && (video_par->lookaheadDepth > 0 || video_par->gopSize != 1)) {
        LOG_ERROR(ENC, "NOT support multislice for 2-pass or gopSize is not 1.");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->initQp != VMPP_ENC_DEFAULT_PAR && video_par->initQp > 51) {
        LOG_ERROR(ENC, "Invalid initQp.");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->rotation > 0 && video_par->rotation > 3) {
        LOG_ERROR(ENC, "Invalid rotation.");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->smartEnc > 5) {
        LOG_ERROR(ENC, "Invalid smartEnc value! Valid range: [0, 5]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->disableMMCO > 0 && (video_par->gopSize != 1 || !IS_H264(ctx->codecFormat))) {
        LOG_ERROR(ENC, "disableMMCO only support gopSize = 1 for h264.");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->inLoopDSRatio != VMPP_ENC_DEFAULT_PAR && video_par->inLoopDSRatio > 1) {
        LOG_ERROR(ENC, "Invalid inLoopDSRatio! Valid range: [0, 1]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->aq_mode != VMPP_ENC_DEFAULT_PAR && (video_par->aq_mode > 3)) {
        LOG_ERROR(ENC, "Invalid aq_mode! Valid range: [0, 3]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->psyFactor != VMPP_ENC_DEFAULT_PAR && (video_par->psyFactor < 0.0 || video_par->psyFactor > 4.0)) {
        LOG_ERROR(ENC, "Invalid psyFactor! Valid range: [0.0, 4.0]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->rdoLevel != VMPP_ENC_DEFAULT_PAR && (video_par->rdoLevel < 1 || video_par->rdoLevel > 3)) {
        LOG_ERROR(ENC, "Invalid rdoLevel! Valid range: [1, 3]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->enableRdoQuant != VMPP_ENC_DEFAULT_PAR && video_par->enableRdoQuant && IS_AV1(ctx->codecFormat) ) {
        LOG_WARN(ENC, "RDO Quant is not supported by this HW version for AV1 encoder. Disable RDO Quant!");
        video_par->enableRdoQuant = 0;
    }   

    if (video_par->qCompress != VMPP_ENC_DEFAULT_PAR && (video_par->qCompress < 0.0 || video_par->qCompress > 1.0)) {
        LOG_ERROR(ENC, "Invalid qCompress! Valid range: [0.0, 1.0]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    if (video_par->bitRateBalanceLevel != VMPP_ENC_DEFAULT_PAR && video_par->bitRateBalanceLevel > 4) {
        LOG_ERROR(ENC, "Invalid bitRateBalanceLevel! Valid range: [0, 4]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

     if (video_par->iQpFactor != VMPP_ENC_DEFAULT_PAR && (video_par->iQpFactor < 0.0 || video_par->iQpFactor > 1.0)) {
        LOG_ERROR(ENC, "Invalid iQpFactor! Valid range: [0.0, 1.0]");
        return vmpp_RSLT_ERR_UNSUPPORTED;
    }

    return vmpp_RSLT_OK;
}

static vmppResult setupGop(struct video_encoder_private_context *ctx, struct va_enc_channel *chn,
                           encChannelParameters *param)
{
    vmppResult ret = vmpp_RSLT_OK;
    chn->params.videoConfig.gopSize = VA_MIN(chn->params.videoConfig.gopSize, MAX_GOP_SIZE);
    if (chn->params.videoConfig.gopSize == 0 && ctx->gopLowdelay /*0*/) {
        chn->params.videoConfig.gopSize = 4;
    }
    memset(ctx->gopPicCfg_tmp, 0, sizeof(ctx->gopPicCfg_tmp));
    ctx->encIn->gopConfig.pGopPicCfg = ctx->gopPicCfg_tmp;
    memset(ctx->gopPicSpecialCfg_tmp, 0, sizeof(ctx->gopPicSpecialCfg_tmp));
    ctx->encIn->gopConfig.pGopPicSpecialCfg = ctx->gopPicSpecialCfg_tmp;

    ret = InitGopConfigs(ctx, chn, 0);
    
    if (ret != vmpp_RSLT_OK)
        return ret;

    if (param->videoConfig.lookaheadDepth) {
        memset(ctx->gopPicCfgPass2_tmp, 0, sizeof(ctx->gopPicCfgPass2_tmp));
        ctx->encIn->gopConfig.pGopPicCfg = ctx->gopPicCfgPass2_tmp;
        ctx->encIn->gopConfig.size = 0;
        memset(ctx->gopPicSpecialCfg_tmp, 0, sizeof(ctx->gopPicSpecialCfg_tmp));
        ctx->encIn->gopConfig.pGopPicSpecialCfg = ctx->gopPicSpecialCfg_tmp;

        ctx->gopLowdelay = 0;
        ret = InitGopConfigs(ctx, chn, 1);
        if (ret != vmpp_RSLT_OK)
            return ret;
        
        ctx->encIn->gopConfig.pGopPicCfgPass1 = ctx->gopPicCfg_tmp;
        ctx->encIn->gopConfig.pGopPicCfg = ctx->encIn->gopConfig.pGopPicCfgPass2 =
            ctx->gopPicCfgPass2_tmp;
    }
    return vmpp_RSLT_OK;
}

static vmppResult prepareConfig(struct video_encoder_private_context *ctx,
                                encChannelParameters *param,
                                VCEncConfig *cfgOut)
{
    encVideoConfiguration *video_par;
    video_par = &param->videoConfig;

    if (video_par->rotation && video_par->rotation != 3)
    {
        cfgOut->width = video_par->height;
        cfgOut->height = video_par->width;
    }
    else {
        cfgOut->width = video_par->width;
        cfgOut->height = video_par->height;
    }
    cfgOut->frameRateDenom = video_par->frameRate.denominator;
    cfgOut->frameRateNum = video_par->frameRate.numerator;
    cfgOut->strongIntraSmoothing = 0;
    cfgOut->streamType = VCENC_BYTE_STREAM; // todo
    cfgOut->level = levelPar2Internal(video_par->level);
    cfgOut->tier = VCENC_HEVC_MAIN_TIER;
    cfgOut->profile = profilePar2Internal(video_par->profile);
    cfgOut->codecFormat = ctx->codecFormat;
    /* VCEncInit() creates its own EWL instance with these device names. Without them the
     * EWL falls back to its compile time defaults (/tmp/dev/vsi_vcx, /tmp/dev/memalloc). */
    cfgOut->enc_dev = param->encDevice;
    cfgOut->mem_dev = param->memDevice;
    cfgOut->bitDepthLuma = video_par->bitDepthLuma;
    cfgOut->bitDepthChroma = video_par->bitDepthChroma;
    cfgOut->maxTLayers = 1;
    if (video_par->keyInt == 1) {
        cfgOut->refFrameAmount = 0;
    } else {
        uint32_t maxRefPics = 0;
        int32_t maxTemporalId = 0;
        int idx;
        for (idx = 0; idx < ctx->encIn->gopConfig.size; idx++) {
            VCEncGopPicConfig *cfg = &(ctx->encIn->gopConfig.pGopPicCfg[idx]);
            if (cfg->codingType != VCENC_INTRA_FRAME) {
                if (maxRefPics < cfg->numRefPics)
                    maxRefPics = cfg->numRefPics;

                if (maxTemporalId < cfg->temporalId)
                    maxTemporalId = cfg->temporalId;
            }
        }
        cfgOut->refFrameAmount = video_par->svcTLayers == 4 ? 4 * (video_par->P2B + 1):
            maxRefPics /*+ cml->interlacedFrame*/ + ctx->encIn->gopConfig.ltrcnt;
        cfgOut->maxTLayers = maxTemporalId + 1;
        // if (cml->flexRefs != NULL) {
        //     cfgOut->refFrameAmount = 4;
        //     cfgOut->maxTLayers = 4;
        // }
    }
    cfgOut->compressor = 3;
    cfgOut->interlacedFrame = 0;     // param->videoConfig.interlacedFrame;
    cfgOut->enableOutputCuInfo = param->videoConfig.enableOutputCuInfo;
    cfgOut->cuInfoVersion = -1;
    cfgOut->enableOutputCtbBits = 0; // param->videoConfig.enableOutputCtbBits;
    cfgOut->rdoLevel = 1;            // CLIP3(1, 3, param->videoConfig.rdoLevel) - 1;
    cfgOut->verbose = 0;
    cfgOut->exp_of_input_alignment = 1;
    if (video_par->alignmentEnable == 0) {
        cfgOut->exp_of_input_alignment = 0;
    }

    cfgOut->exp_of_ref_alignment = 6;
    cfgOut->exp_of_ref_ch_alignment = 6;
    cfgOut->exp_of_aqinfo_alignment = 6;
    cfgOut->exteralReconAlloc = 0;
    cfgOut->P010RefEnable = 0;
    cfgOut->enableSsim = 1; // param->videoConfig.enableSSIM;
    cfgOut->enablePsnr = 1; // param->videoConfig.enablePSNR;
    cfgOut->ctbRcMode = param->videoConfig.llRc > 0 ? 2 : 0;
    cfgOut->ctbRcMode += (video_par->lookaheadDepth == 0 && (video_par->tune == 1 || video_par->tune == 2 || video_par->tune == 3));
    /* The channel parameters do not carry a core mode in this SDK version. */
    cfgOut->parallelCoreNum = ctx->workmode == MULTI_CORE_MODE ? MAX_CORE_NUM : 1;
    if (cfgOut->parallelCoreNum > 1 && cfgOut->width * cfgOut->height < 256 * 256 * 2 * 2 && video_par->lookaheadDepth > 0) {
        LOG_WARN(ENC, "Use single core for small resolution");
        cfgOut->parallelCoreNum = 1;
    }
    if (cfgOut->parallelCoreNum > 1 && cfgOut->width * cfgOut->height < 256 * 256 && video_par->lookaheadDepth == 0) {
        LOG_WARN(ENC, "Use single core for small resolution");
        cfgOut->parallelCoreNum = 1;
    }
    if (cfgOut->parallelCoreNum == 1) {
        LOG_INFO(ENC, "enc work mode SINLE_CORE_MODE.");
    } else {
        LOG_INFO(ENC, "enc work mode MULTI_CORE_MODE.");
    }

    cfgOut->pass = video_par->lookaheadDepth ? 2 : 0;
    cfgOut->lookaheadDepth = video_par->lookaheadDepth;
    cfgOut->bPass1AdaptiveGop = (video_par->gopSize == 0);
    cfgOut->picOrderCntType = 0;
    cfgOut->dumpRegister = 0;
    cfgOut->dumpCuInfo = 0;
    cfgOut->dumpCtbBits = 0;
    cfgOut->rasterscan = 0;
    cfgOut->log2MaxPicOrderCntLsb = 16; // param->videoConfig.log2MaxPicOrderCntLsb;
    cfgOut->log2MaxFrameNum = 12;       // param->videoConfig.log2MaxFrameNum;
    cfgOut->extDSRatio = 0; //(param->videoConfig.lookaheadDepth && cml->halfDsInput ? 1 : 0);
    cfgOut->inLoopDSRatio = 0;
    if (video_par->inLoopDSRatio == VMPP_ENC_DEFAULT_PAR) {
        if (cfgOut->lookaheadDepth)
            cfgOut->inLoopDSRatio = 1;
    } else {
        cfgOut->inLoopDSRatio = video_par->inLoopDSRatio;
    }
    if (cfgOut->width < 272 || cfgOut->height < 256)
        cfgOut->inLoopDSRatio = 0;
    cfgOut->cuInfoVersion = -1;
    /*external SRAM*/
    cfgOut->extSramLumHeightBwd =
        IS_H264(ctx->codecFormat) ? 12 : (IS_HEVC(ctx->codecFormat) ? 16 : 0);
    cfgOut->extSramChrHeightBwd =
        IS_H264(ctx->codecFormat) ? 6 : (IS_HEVC(ctx->codecFormat) ? 8 : 0);
    cfgOut->extSramLumHeightFwd =
        IS_H264(ctx->codecFormat) ? 12 : (IS_HEVC(ctx->codecFormat) ? 16 : 0);
    cfgOut->extSramChrHeightFwd =
        IS_H264(ctx->codecFormat) ? 6 : (IS_HEVC(ctx->codecFormat) ? 8 : 0);
    cfgOut->AXIAlignment = 0;
    cfgOut->aifEnable = 0;
    cfgOut->slice_idx = 0;
    cfgOut->gopSize = video_par->gopSize;
    cfgOut->codedChromaIdc = VCENC_CHROMA_IDC_420;
    cfgOut->writeReconToDDR = 1;
    cfgOut->TxTypeSearchEnable = 0;
    cfgOut->av1InterFiltSwitch = 1;
    cfgOut->gopMaxBSize = video_par->maxBFrames;
    cfgOut->tune = TunePar2Internal(video_par->tune);

    ctx->preset = video_par->preset;
    ctx->gdrDuration = video_par->gdrDuration;
    Parameter_Preset(cfgOut, video_par->preset, &ctx->enableRdoQuant);

    if (video_par->rdoLevel != VMPP_ENC_DEFAULT_PAR)
        cfgOut->rdoLevel = CLIP3(1, 3, video_par->rdoLevel) - 1;

    if (video_par->width * video_par->height < 300000)
        cfgOut->rdoLevel = MAX(1, cfgOut->rdoLevel);

    ctx->enProfiling = param->enProfiling;
    return vmpp_RSLT_OK;
}

static vmppResult initEncoder(struct video_encoder_private_context *ctx, struct va_enc_channel *chn,
                              encChannelParameters *param)
{
    VCEncRet encRet;
    VCEncConfig cfg;
    memset(&cfg, 0, sizeof(VCEncConfig));
    prepareConfig(ctx, param, &cfg);
    if ((encRet = VCEncInit(&cfg, (VCEncInst *)&(chn->codec_inst), NULL)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncInit() failed. %d", encRet);
        if (encRet == VCENC_NO_CHANNEL) {
            return vmpp_RSLT_ERR_ALLOC_CHANNEL;
        }
        return vmpp_RSLT_ERR_ENC_INIT;
    }

    memcpy(&ctx->cfg, &cfg, sizeof(cfg));

    return vmpp_RSLT_OK;
}

static vmppResult reinitEncoder(struct video_encoder_private_context *ctx,
                                struct va_enc_channel *chn, encChannelParameters *param)
{
    VCEncRet encRet;
    VCEncConfig cfg;
    memset(&cfg, 0, sizeof(VCEncConfig));
    prepareConfig(ctx, param, &cfg);

    /* The library does not provide VCEncReconfig(), the instance is restarted. */
    if (chn->codec_inst) {
        VCEncRelease((VCEncInst)chn->codec_inst);
        chn->codec_inst = NULL;
    }

    if ((encRet = VCEncInit(&cfg, (VCEncInst *)&(chn->codec_inst), NULL)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncInit() failed. %d", encRet);
        if (encRet == VCENC_NO_CHANNEL) {
            return vmpp_RSLT_ERR_ALLOC_CHANNEL;
        }
        return vmpp_RSLT_ERR_ENC_INIT;
    }

    memcpy(&ctx->cfg, &cfg, sizeof(cfg));

    return vmpp_RSLT_OK;
}

static vmppResult setupCodingConfig(struct video_encoder_private_context *ctx,
                                    struct va_enc_channel *chn, const vmppFrame *frame)
{
    VCEncRet encRet;
    VCEncCodingCtrl codingCfg;
    memset(&codingCfg, 0, sizeof(VCEncCodingCtrl));

    if ((encRet = VCEncGetCodingCtrl(chn->codec_inst, &codingCfg)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncGetCodingCtrl failed: %d", encRet);
        VCEncRelease(chn->codec_inst);
        chn->codec_inst = NULL;
        return vmpp_RSLT_ERR_ENC_INIT;
    } else {
        codingCfg.sliceSize = ctx->sliceSize;

        if (chn->params.videoConfig.profile == vmpp_VIDEO_PRFL_H264_BASELINE) {
            codingCfg.enableCabac = 0;
        } else {
            codingCfg.enableCabac = 1;
        }

        codingCfg.cabacInitFlag = 0;
        codingCfg.vuiVideoFullRange = 0;
        // codingCfg.enableRdoQuant = 1; // DEFAULT
        if (IS_H264(ctx->codecFormat)) {
            codingCfg.layerInRefIdcEnable = 0;
        }
        codingCfg.sramPowerdownDisable = 0;
        codingCfg.disableDeblockingFilter = 0;
        codingCfg.tc_Offset = 0;
        codingCfg.beta_Offset = 0;
        codingCfg.enableSao = 1;
        codingCfg.enableDeblockOverride = 0;
        codingCfg.deblockOverride = 0;
        codingCfg.enableDynamicRdo = 0;
        codingCfg.dynamicRdoCu16Bias = 3;
        codingCfg.dynamicRdoCu16Factor = 80;
        codingCfg.dynamicRdoCu32Bias = 2;
        codingCfg.dynamicRdoCu32Factor = 32;
        codingCfg.seiMessages = 0;
        codingCfg.gdrDuration = ctx->gdrDuration;
        codingCfg.fieldOrder = 0;
        codingCfg.cirStart = 0;
        codingCfg.cirInterval = 0;
        if (codingCfg.gdrDuration == 0) {
            codingCfg.intraArea.top = -1;
            codingCfg.intraArea.left = -1;
            codingCfg.intraArea.bottom = -1;
            codingCfg.intraArea.right = -1;
            codingCfg.intraArea.enable = CheckArea(&codingCfg.intraArea, frame, ctx->codecFormat);
        } else {
            codingCfg.intraArea.enable = 0;
        }
        codingCfg.pcm_loop_filter_disabled_flag = 0;

        codingCfg.ipcm1Area.top = -1;
        codingCfg.ipcm1Area.left = -1;
        codingCfg.ipcm1Area.bottom = -1;
        codingCfg.ipcm1Area.right = -1;
        codingCfg.ipcm1Area.enable = CheckArea(&codingCfg.ipcm1Area, frame, ctx->codecFormat);

        codingCfg.ipcm2Area.top = -1;
        codingCfg.ipcm2Area.left = -1;
        codingCfg.ipcm2Area.bottom = -1;
        codingCfg.ipcm2Area.right = -1;
        codingCfg.ipcm2Area.enable = CheckArea(&codingCfg.ipcm2Area, frame, ctx->codecFormat);

        codingCfg.ipcm3Area.top = -1;
        codingCfg.ipcm3Area.left = -1;
        codingCfg.ipcm3Area.bottom = -1;
        codingCfg.ipcm3Area.right = -1;
        codingCfg.ipcm3Area.enable = CheckArea(&codingCfg.ipcm3Area, frame, ctx->codecFormat);

        codingCfg.ipcm4Area.top = -1;
        codingCfg.ipcm4Area.left = -1;
        codingCfg.ipcm4Area.bottom = -1;
        codingCfg.ipcm4Area.right = -1;
        codingCfg.ipcm4Area.enable = CheckArea(&codingCfg.ipcm4Area, frame, ctx->codecFormat);

        codingCfg.ipcm5Area.top = -1;
        codingCfg.ipcm5Area.left = -1;
        codingCfg.ipcm5Area.bottom = -1;
        codingCfg.ipcm5Area.right = -1;
        codingCfg.ipcm5Area.enable = CheckArea(&codingCfg.ipcm5Area, frame, ctx->codecFormat);

        codingCfg.ipcm6Area.top = -1;
        codingCfg.ipcm6Area.left = -1;
        codingCfg.ipcm6Area.bottom = -1;
        codingCfg.ipcm6Area.right = -1;
        codingCfg.ipcm6Area.enable = CheckArea(&codingCfg.ipcm6Area, frame, ctx->codecFormat);

        codingCfg.ipcm7Area.top = -1;
        codingCfg.ipcm7Area.left = -1;
        codingCfg.ipcm7Area.bottom = -1;
        codingCfg.ipcm7Area.right = -1;
        codingCfg.ipcm7Area.enable = CheckArea(&codingCfg.ipcm7Area, frame, ctx->codecFormat);

        codingCfg.ipcm8Area.top = -1;
        codingCfg.ipcm8Area.left = -1;
        codingCfg.ipcm8Area.bottom = -1;
        codingCfg.ipcm8Area.right = -1;
        codingCfg.ipcm8Area.enable = CheckArea(&codingCfg.ipcm8Area, frame, ctx->codecFormat);

        codingCfg.ipcmMapEnable = 0;
        codingCfg.pcm_enabled_flag =
            (codingCfg.ipcm1Area.enable || codingCfg.ipcm2Area.enable ||
             codingCfg.ipcm3Area.enable || codingCfg.ipcm4Area.enable ||
             codingCfg.ipcm5Area.enable || codingCfg.ipcm6Area.enable ||
             codingCfg.ipcm7Area.enable || codingCfg.ipcm8Area.enable || codingCfg.ipcmMapEnable);

        if (codingCfg.gdrDuration == 0) {
            codingCfg.roi1Area.top = 0;
            codingCfg.roi1Area.left = 0;
            codingCfg.roi1Area.bottom = 0;
            codingCfg.roi1Area.right = 0;
            codingCfg.roi1Area.enable = 0;
        } else {
            codingCfg.roi1Area.enable = 0;
        }

        codingCfg.roi2Area.top = 0;
        codingCfg.roi2Area.left = 0;
        codingCfg.roi2Area.bottom = 0;
        codingCfg.roi2Area.right = 0;
        codingCfg.roi2Area.enable = 0;

        codingCfg.roi3Area.top = 0;
        codingCfg.roi3Area.left = 0;
        codingCfg.roi3Area.bottom = 0;
        codingCfg.roi3Area.right = 0;
        codingCfg.roi3Area.enable = 0;

        codingCfg.roi4Area.top = 0;
        codingCfg.roi4Area.left = 0;
        codingCfg.roi4Area.bottom = 0;
        codingCfg.roi4Area.right = 0;
        codingCfg.roi4Area.enable = 0;

        codingCfg.roi5Area.top = 0;
        codingCfg.roi5Area.left = 0;
        codingCfg.roi5Area.bottom = 0;
        codingCfg.roi5Area.right = 0;
        codingCfg.roi5Area.enable = 0;

        codingCfg.roi6Area.top = 0;
        codingCfg.roi6Area.left = 0;
        codingCfg.roi6Area.bottom = 0;
        codingCfg.roi6Area.right = 0;
        codingCfg.roi6Area.enable = 0;

        codingCfg.roi7Area.top = 0;
        codingCfg.roi7Area.left = 0;
        codingCfg.roi7Area.bottom = 0;
        codingCfg.roi7Area.right = 0;
        codingCfg.roi7Area.enable = 0;

        codingCfg.roi8Area.top = 0;
        codingCfg.roi8Area.left = 0;
        codingCfg.roi8Area.bottom = 0;
        codingCfg.roi8Area.right = 0;
        codingCfg.roi8Area.enable = 0;

        codingCfg.roi1DeltaQp = 0;
        codingCfg.roi2DeltaQp = 0;
        codingCfg.roi3DeltaQp = 0;
        codingCfg.roi4DeltaQp = 0;
        codingCfg.roi5DeltaQp = 0;
        codingCfg.roi6DeltaQp = 0;
        codingCfg.roi7DeltaQp = 0;
        codingCfg.roi8DeltaQp = 0;
        codingCfg.roi1Qp = -255;
        codingCfg.roi2Qp = -255;
        codingCfg.roi3Qp = -255;
        codingCfg.roi4Qp = -255;
        codingCfg.roi5Qp = -255;
        codingCfg.roi6Qp = -255;
        codingCfg.roi7Qp = -255;
        codingCfg.roi8Qp = -255;
        codingCfg.roiMapDeltaQpEnable = ctx->roiMapDeltaQpEnable;
        codingCfg.roiMapDeltaQpBlockUnit = ctx->roiMapDeltaQpBlockUnit;
        codingCfg.RoimapCuCtrl_index_enable = 0;
        codingCfg.RoimapCuCtrl_enable = 0;
        codingCfg.RoimapCuCtrl_ver = 0;
        codingCfg.RoiQpDelta_ver = 1;
        codingCfg.skipMapEnable = 0;
        codingCfg.rdoqMapEnable = 0;
        codingCfg.enableScalingList = 0;
        codingCfg.chroma_qp_offset = 0;
        codingCfg.inputLineBufEn = 0;
        codingCfg.inputLineBufLoopBackEn = 0;
        codingCfg.amountPerLoopBack = 0;
        codingCfg.inputLineBufHwModeEn = 0;
        codingCfg.inputLineBufCbFunc = NULL;
        codingCfg.inputLineBufCbData = NULL;

        /*stream multi-segment*/
        codingCfg.streamMultiSegmentMode = 0;
        codingCfg.streamMultiSegmentAmount = 4;
        codingCfg.streamMultiSegCbFunc = NULL;
        codingCfg.streamMultiSegCbData = NULL;

        codingCfg.noiseReductionEnable = 0;
        codingCfg.noiseLow = 10;
        codingCfg.firstFrameSigma = 11;

        /* smart */
        codingCfg.smartModeEnable = 0;
        codingCfg.smartH264LumDcTh = 5;
        codingCfg.smartH264CbDcTh = 1;
        codingCfg.smartH264CrDcTh = 1;
        for (int i = 0; i < 3; i++) {
            codingCfg.smartHevcLumDcTh[i] = 2;
            codingCfg.smartHevcChrDcTh[i] = 2;
        }
        codingCfg.smartHevcLumAcNumTh[0] = 12;
        codingCfg.smartHevcLumAcNumTh[1] = 51;
        codingCfg.smartHevcLumAcNumTh[2] = 204;
        codingCfg.smartHevcChrAcNumTh[0] = 3;
        codingCfg.smartHevcChrAcNumTh[1] = 12;
        codingCfg.smartHevcChrAcNumTh[2] = 51;
        codingCfg.smartH264Qp = 30;
        codingCfg.smartHevcLumQp = 30;
        codingCfg.smartHevcChrQp = 30;
        for (int i = 0; i < 4; i++) codingCfg.smartMeanTh[i] = 5;
        codingCfg.smartPixNumCntTh = 0;

        /* The tile configuration belongs to VCEncConfig and is applied
           before VCEncInit(), it is therefore not modified here. */

        codingCfg.Hdr10Display.hdr10_display_enable = 0;
        // TODO: hdr10 display details

        codingCfg.Hdr10LightLevel.hdr10_lightlevel_enable = 0;
        // TODO: hdr10 light level details

        codingCfg.vuiColorDescription.vuiColorDescripPresentFlag = 0;
        if (codingCfg.vuiColorDescription.vuiColorDescripPresentFlag) {
            codingCfg.vuiColorDescription.vuiMatrixCoefficients = 9;
            codingCfg.vuiColorDescription.vuiColorPrimaries = 9;
            codingCfg.vuiColorDescription.vuiTransferCharacteristics = 0;
        }
        codingCfg.vuiVideoFormat = 5;
        codingCfg.vuiVideoSignalTypePresentFlag = 0;
        codingCfg.sampleAspectRatioHeight = 0;
        codingCfg.sampleAspectRatioWidth = 0;

        codingCfg.RpsInSliceHeader = 0;
        codingCfg.meVertSearchRange = (ctx->preset == 2  && IS_H264(ctx->codecFormat)) ? 24 : 0;
        if (chn->params.videoConfig.enableRdoQuant != VMPP_ENC_DEFAULT_PAR)
            codingCfg.enableRdoQuant = chn->params.videoConfig.enableRdoQuant;
        else
            codingCfg.enableRdoQuant = ctx->enableRdoQuant;
#if 0
        if (ctx->cfg.tune == VCENC_TUNE_PSNR) {
            codingCfg.aq_mode = 0;
            codingCfg.psyFactor = 0;
            codingCfg.aq_strength = 0;
        } else if (ctx->cfg.tune == VCENC_TUNE_SSIM) {
            codingCfg.aq_mode = 2;
            codingCfg.psyFactor = 0;
            codingCfg.aq_strength = 0.7;
        } else if (ctx->cfg.tune == VCENC_TUNE_VISUAL) {
            codingCfg.aq_mode = 2;
            codingCfg.psyFactor = 0.75; // 1
            codingCfg.aq_strength = 0.7;
        } else if (ctx->cfg.tune == VCENC_TUNE_SHARP_VISUAL) {
            codingCfg.aq_mode = 2;
            codingCfg.psyFactor = 0.75; // 1
            codingCfg.aq_strength = 0.7;
            codingCfg.enableRdoQuant = 0;
        }
#endif
        if (chn->params.videoConfig.aq_mode != VMPP_ENC_DEFAULT_PAR)
            codingCfg.aq_mode = chn->params.videoConfig.aq_mode;
        if (chn->params.videoConfig.aq_strength != VMPP_ENC_DEFAULT_PAR)
            codingCfg.aq_strength = chn->params.videoConfig.aq_strength;
        if (chn->params.videoConfig.psyFactor != VMPP_ENC_DEFAULT_PAR)
            codingCfg.psyFactor = chn->params.videoConfig.psyFactor;

        /* The realtime ROI control is not provided by this library version. */

        if ((encRet = VCEncSetCodingCtrl(chn->codec_inst, &codingCfg)) != VCENC_OK) {
            LOG_ERROR(ENC, "VCEncSetCodingCtrl() failed: %d", encRet);
            VCEncRelease(chn->codec_inst);
            chn->codec_inst = NULL;
            return vmpp_RSLT_ERR_ENC_INIT;
        }
        memcpy(&ctx->codingCfg, &codingCfg, sizeof(codingCfg));
    }
    return vmpp_RSLT_OK;
}

static int checkQP(const vmppEncPictureROI *roi)
{
    if (!roi)
        return 0;
    if (roi->qpType == vmpp_ENC_QP)
        return (roi->qpValue >= 0);
    else if (roi->qpType == vmpp_ENC_QP_DELTA)
        return roi->qpValue;
    return 0;
}

static void checkROIValidation(vmppFrame *frame, vmppEncPictureROI roi[VMPP_ENC_MAX_ROI_NUM])
{
    for (int i = 0; i < VMPP_ENC_MAX_ROI_NUM; i++) {
        if (roi[i].area.top > frame->height)
            roi[i].area.top = frame->height;

        if (roi[i].area.left > frame->width)
            roi[i].area.left = frame->width;

        if (roi[i].area.bottom > frame->height)
            roi[i].area.bottom = frame->height;
        if (roi[i].area.bottom < roi[i].area.top)
            roi[i].area.bottom = roi[i].area.top;

        if (roi[i].area.right > frame->width)
            roi[i].area.right = frame->width;
        if (roi[i].area.right < roi[i].area.left)
            roi[i].area.right = roi[i].area.left;
    }
}

static inline uint32_t roundDown4Area(uint32_t value, uint32_t blockSize)
{
    return (value / blockSize);
}

static inline uint32_t roundUp4Area(uint32_t value, uint32_t blockSize)
{
    return ((value + blockSize - 1) / blockSize);
}

static vmppResult setupROI(struct video_encoder_private_context *ctx, struct va_enc_channel *chn,
                           EncInputBuffer *input, vmppEncPictureROI roi[VMPP_ENC_MAX_ROI_NUM])
{
    VCEncRet encRet;
    VCEncCodingCtrl codingCfg;
    memset(&codingCfg, 0, sizeof(VCEncCodingCtrl));
    vmppFrame dummyFrame = {0};
    dummyFrame.width = input->width;
    dummyFrame.height = input->height;
    vmppFrame *frame = &dummyFrame;

    if ((encRet = VCEncGetCodingCtrl(chn->codec_inst, &codingCfg)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncGetCodingCtrl failed: %d", encRet);
        VCEncRelease(chn->codec_inst);
        chn->codec_inst = NULL;
        return vmpp_RSLT_ERR_ENC_CODING_CTRL;
    } else {
        checkROIValidation(frame, roi);
        uint32_t blockSize = 64;
        if (IS_H264(ctx->codecFormat))
            blockSize = 16;
        memcpy(&codingCfg, &ctx->codingCfg, sizeof(VCEncCodingCtrl));
        if (codingCfg.gdrDuration == 0) {
            if (roi[0].area.enable) {
                codingCfg.roi1Area.top = roundDown4Area(roi[0].area.top, blockSize);
                codingCfg.roi1Area.left = roundDown4Area(roi[0].area.left, blockSize);
                codingCfg.roi1Area.bottom = roundUp4Area(roi[0].area.bottom, blockSize);
                codingCfg.roi1Area.right = roundUp4Area(roi[0].area.right, blockSize);
                codingCfg.roi1Area.enable =
                    CheckArea(&codingCfg.roi1Area, frame, ctx->codecFormat) && checkQP(&roi[0]);
                codingCfg.roi1DeltaQp = roi[0].qpType == vmpp_ENC_QP_DELTA ? roi[0].qpValue : 0;
                codingCfg.roi1Qp = roi[0].qpType == vmpp_ENC_QP ? roi[0].qpValue : -255;
            } else {
                codingCfg.roi1Area.enable = 0;
            }
        } else {
            codingCfg.roi1Area.enable = 0;
        }
        if (roi[1].area.enable) {
            codingCfg.roi2Area.top = roundDown4Area(roi[1].area.top, blockSize);
            codingCfg.roi2Area.left = roundDown4Area(roi[1].area.left, blockSize);
            codingCfg.roi2Area.bottom = roundUp4Area(roi[1].area.bottom, blockSize);
            codingCfg.roi2Area.right = roundUp4Area(roi[1].area.right, blockSize);
            codingCfg.roi2Area.enable =
                CheckArea(&codingCfg.roi2Area, frame, ctx->codecFormat) && checkQP(&roi[1]);
            codingCfg.roi2DeltaQp = roi[1].qpType == vmpp_ENC_QP_DELTA ? roi[1].qpValue : 0;
            codingCfg.roi2Qp = roi[1].qpType == vmpp_ENC_QP ? roi[1].qpValue : -255;
        } else {
            codingCfg.roi2Area.enable = 0;
        }

        if (roi[2].area.enable) {
            codingCfg.roi3Area.top = roundDown4Area(roi[2].area.top, blockSize);
            codingCfg.roi3Area.left = roundDown4Area(roi[2].area.left, blockSize);
            codingCfg.roi3Area.bottom = roundUp4Area(roi[2].area.bottom, blockSize);
            codingCfg.roi3Area.right = roundUp4Area(roi[2].area.right, blockSize);
            codingCfg.roi3Area.enable =
                CheckArea(&codingCfg.roi3Area, frame, ctx->codecFormat) && checkQP(&roi[2]);
            codingCfg.roi3DeltaQp = roi[2].qpType == vmpp_ENC_QP_DELTA ? roi[2].qpValue : 0;
            codingCfg.roi3Qp = roi[2].qpType == vmpp_ENC_QP ? roi[2].qpValue : -255;
        } else {
            codingCfg.roi3Area.enable = 0;
        }

        if (roi[3].area.enable) {
            codingCfg.roi4Area.top = roundDown4Area(roi[3].area.top, blockSize);
            codingCfg.roi4Area.left = roundDown4Area(roi[3].area.left, blockSize);
            codingCfg.roi4Area.bottom = roundUp4Area(roi[3].area.bottom, blockSize);
            codingCfg.roi4Area.right = roundUp4Area(roi[3].area.right, blockSize);
            codingCfg.roi4Area.enable =
                CheckArea(&codingCfg.roi4Area, frame, ctx->codecFormat) && checkQP(&roi[3]);
            codingCfg.roi4DeltaQp = roi[3].qpType == vmpp_ENC_QP_DELTA ? roi[3].qpValue : 0;
            codingCfg.roi4Qp = roi[3].qpType == vmpp_ENC_QP ? roi[3].qpValue : -255;
        } else {
            codingCfg.roi4Area.enable = 0;
        }

        if (roi[4].area.enable) {
            codingCfg.roi5Area.top = roundDown4Area(roi[4].area.top, blockSize);
            codingCfg.roi5Area.left = roundDown4Area(roi[4].area.left, blockSize);
            codingCfg.roi5Area.bottom = roundUp4Area(roi[4].area.bottom, blockSize);
            codingCfg.roi5Area.right = roundUp4Area(roi[4].area.right, blockSize);
            codingCfg.roi5Area.enable =
                CheckArea(&codingCfg.roi5Area, frame, ctx->codecFormat) && checkQP(&roi[4]);
            codingCfg.roi5DeltaQp = roi[4].qpType == vmpp_ENC_QP_DELTA ? roi[4].qpValue : 0;
            codingCfg.roi5Qp = roi[4].qpType == vmpp_ENC_QP ? roi[4].qpValue : -255;
        } else {
            codingCfg.roi5Area.enable = 0;
        }

        if (roi[5].area.enable) {
            codingCfg.roi6Area.top = roundDown4Area(roi[5].area.top, blockSize);
            codingCfg.roi6Area.left = roundDown4Area(roi[5].area.left, blockSize);
            codingCfg.roi6Area.bottom = roundUp4Area(roi[5].area.bottom, blockSize);
            codingCfg.roi6Area.right = roundUp4Area(roi[5].area.right, blockSize);
            codingCfg.roi6Area.enable =
                CheckArea(&codingCfg.roi6Area, frame, ctx->codecFormat) && checkQP(&roi[5]);
            codingCfg.roi6DeltaQp = roi[5].qpType == vmpp_ENC_QP_DELTA ? roi[5].qpValue : 0;
            codingCfg.roi6Qp = roi[5].qpType == vmpp_ENC_QP ? roi[5].qpValue : -255;
        } else {
            codingCfg.roi6Area.enable = 0;
        }

        if (roi[6].area.enable) {
            codingCfg.roi7Area.top = roundDown4Area(roi[6].area.top, blockSize);
            codingCfg.roi7Area.left = roundDown4Area(roi[6].area.left, blockSize);
            codingCfg.roi7Area.bottom = roundUp4Area(roi[6].area.bottom, blockSize);
            codingCfg.roi7Area.right = roundUp4Area(roi[6].area.right, blockSize);
            codingCfg.roi7Area.enable =
                CheckArea(&codingCfg.roi7Area, frame, ctx->codecFormat) && checkQP(&roi[6]);
            codingCfg.roi7DeltaQp = roi[6].qpType == vmpp_ENC_QP_DELTA ? roi[6].qpValue : 0;
            codingCfg.roi7Qp = roi[6].qpType == vmpp_ENC_QP ? roi[6].qpValue : -255;
        } else {
            codingCfg.roi7Area.enable = 0;
        }

        if (roi[7].area.enable) {
            codingCfg.roi8Area.top = roundDown4Area(roi[7].area.top, blockSize);
            codingCfg.roi8Area.left = roundDown4Area(roi[7].area.left, blockSize);
            codingCfg.roi8Area.bottom = roundUp4Area(roi[7].area.bottom, blockSize);
            codingCfg.roi8Area.right = roundUp4Area(roi[7].area.right, blockSize);
            codingCfg.roi8Area.enable =
                CheckArea(&codingCfg.roi8Area, frame, ctx->codecFormat) && checkQP(&roi[7]);
            codingCfg.roi8DeltaQp = roi[7].qpType == vmpp_ENC_QP_DELTA ? roi[7].qpValue : 0;
            codingCfg.roi8Qp = roi[7].qpType == vmpp_ENC_QP ? roi[7].qpValue : -255;
        } else {
            codingCfg.roi8Area.enable = 0;
        }

        if ((encRet = VCEncSetCodingCtrl(chn->codec_inst, &codingCfg)) != VCENC_OK) {
            //LOG_ERROR(ENC, "VCEncSetCodingCtrl() failed: %d", encRet);
            //VCEncRelease(chn->codec_inst);
            //chn->codec_inst = NULL;
            return vmpp_RSLT_ERR_ENC_CODING_CTRL;
        }
    }
    return vmpp_RSLT_OK;
}

static uint32_t getSuitableSEIBufferSize(vmppFrame *frame, int forOutput)
{
    uint32_t size = 0;
    if (frame) {
        if (forOutput) /*enough for sei header?*/
            size += (frame->seiCount * 32);
        for (uint32_t i = 0; i < frame->seiCount; i++) {
            size += frame->seiData[i]->payloadDataSize;
        }

        size = ALLIGN(size, 1024);
    }
    return size;
}

static vmppResult saveSEI(struct video_encoder_private_context *ctx, EncInputBuffer *inputBuffer,
                          vmppFrame *frame)
{
    // extParams
    vmppResult ret = vmpp_RSLT_OK;
    uint32_t inSize = 0, outSize = 0;
    uint8_t *in = NULL, *out = NULL;
    uint32_t offset = 0;
    uint32_t newSize = sizeof(ExternalSEI) * frame->seiCount;
    ExternalSEI *tmp = inputBuffer->extSEI;
    if (!inputBuffer->extSEI || (inputBuffer->extSEI && inputBuffer->extSEIBufferSize < newSize)) {
        tmp = (ExternalSEI *)realloc(inputBuffer->extSEI, sizeof(ExternalSEI) * frame->seiCount);
        if (!tmp) {
            LOG_ERROR(ENC, "Fail to realloc memory for extended sei, count %d, old count %d",
                      frame->seiCount, inputBuffer->extSEICount);
            ret = vmpp_RSLT_ERR_NO_MEMORY;
            goto fail;
        }
        inputBuffer->extSEIBufferSize = newSize;
    }
    inputBuffer->extSEI = tmp;
    inputBuffer->extSEICount = frame->seiCount;
    memset(inputBuffer->extSEI, 0, inputBuffer->extSEIBufferSize);
    inSize = getSuitableSEIBufferSize(frame, 0);
    outSize = getSuitableSEIBufferSize(frame, 1);
    in = getIdleSEIBuffer(ctx, inSize);
    if (!in) {
        LOG_ERROR(ENC, "Fail to get idle sei buffer for input");
        ret = vmpp_RSLT_ERR_NO_MEMORY;
        goto fail;
    }

    out = getIdleSEIBuffer(ctx, outSize);
    if (!out) {
        LOG_ERROR(ENC, "Fail to get idle sei buffer for output");
        ret = vmpp_RSLT_ERR_NO_MEMORY;
        goto fail;
    }

    for (uint32_t i = 0; i < frame->seiCount; i++) {
        inputBuffer->extSEI[i].nalType = frame->seiData[i]->nalType;
        inputBuffer->extSEI[i].payloadType = frame->seiData[i]->payloadType;
        inputBuffer->extSEI[i].payloadDataSize = frame->seiData[i]->payloadDataSize;
        inputBuffer->extSEI[i].pPayloadData = in + offset;
        memcpy(inputBuffer->extSEI[i].pPayloadData, frame->seiData[i]->payloadData,
               frame->seiData[i]->payloadDataSize);
        offset += frame->seiData[i]->payloadDataSize;
    }

    inputBuffer->encodedSEI = out;
    inputBuffer->encodedSEIBufferSize = outSize;

    return vmpp_RSLT_OK;
fail:
    if (in)
        setSEIBufferIdle(ctx, in);
    if (out)
        setSEIBufferIdle(ctx, out);
    // Do not free it here, reuse it as much as possible
    // if (inputBuffer->extSEI) {
    //     free(inputBuffer->extSEI);
    //     inputBuffer->extSEI = NULL;
    // }
    // inputBuffer->extSEIBufferSize = 0;
    inputBuffer->extSEICount = 0;

    return ret;
}

static vmppResult resetRateCtrl(struct video_encoder_private_context *ctx, struct va_enc_channel *chn,
                                encChannelParameters *param)
{
    VCEncRet encRet;
    VCEncRateCtrl rcCfg;
    encVideoConfiguration *video_par;
    memset(&rcCfg, 0, sizeof(VCEncRateCtrl));

    video_par = &param->videoConfig;

    if ((encRet = VCEncGetRateCtrl(chn->codec_inst, &rcCfg)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncGetRateCtrl() failed: %d", encRet);
        VCEncRelease(chn->codec_inst);
        chn->codec_inst = NULL;
        return vmpp_RSLT_ERR_ENC_INIT;
    } else {
        rcCfg.qpHdr = -1;
        if (video_par->initQp != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpHdr = video_par->initQp;

        rcCfg.hrd = video_par->hrd;
        rcCfg.hrdCpbSize = 0;
        rcCfg.cpbMaxRate = 0;
        if (video_par->vbvBufSize != VMPP_ENC_DEFAULT_PAR)
            rcCfg.hrdCpbSize = video_par->vbvBufSize;
        if (video_par->vbvMaxRate != VMPP_ENC_DEFAULT_PAR)
            rcCfg.cpbMaxRate = video_par->vbvMaxRate;
        rcCfg.bitPerSecond = video_par->bitRate;
        rcCfg.bitrateWindow = 300;
        rcCfg.bitVarRangeI = 10000;
        rcCfg.bitVarRangeP = 10000;
        rcCfg.bitVarRangeB = 10000;
        rcCfg.smoothPsnrInGOP = 0;
        rcCfg.blockRCSize = 0;
        rcCfg.pictureSkip = video_par->pictureSkip;
        rcCfg.fixedIntraQp = 0;
        rcCfg.crf = video_par->crf;
        rcCfg.frameRateNum = video_par->frameRate.numerator;
        rcCfg.frameRateDenom = video_par->frameRate.denominator;
        if(rcCfg.frameRateNum == 0 || rcCfg.frameRateDenom == 0)
        {
            rcCfg.frameRateNum = 30;
            rcCfg.frameRateDenom = 1;
        }

        if (video_par->llRc != VMPP_ENC_DEFAULT_PAR) {
            switch (video_par->llRc) {
            case 0:
                rcCfg.ctbRc = 0;
                rcCfg.bitrateWindow = 300;
                break;
            case 1:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 5.0;
                rcCfg.tolCtbRcIntra = 5.0;
                rcCfg.bitrateWindow = 20;
                break;
            case 2:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 3.0;
                rcCfg.tolCtbRcIntra = 3.0;
                rcCfg.bitrateWindow = 10;
                break;
            case 3:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 2.0;
                rcCfg.tolCtbRcIntra = 2.0;
                rcCfg.bitrateWindow = 5;
                break;
            case 4:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 1.0;
                rcCfg.tolCtbRcIntra = 1.0;
                rcCfg.bitrateWindow = 5;
                break;
            case 5:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 0.5;
                rcCfg.tolCtbRcIntra = 0.5;
                rcCfg.bitrateWindow = 5;
                break;
            default:
                rcCfg.ctbRc = 0;
            }
            rcCfg.ctbRc += (video_par->lookaheadDepth == 0 && (video_par->tune == 1 || video_par->tune == 2 || video_par->tune == 3));
        }

        if (rcCfg.ctbRc & 2)
        {
            if(video_par->maxFrameSize > 0)
            {
            }
            else
            {
                // default value is (width * height * 1.5 * 8 / 32) bit
            }
        }
        else
        {
        }

        if (video_par->smartEnc > 0 && video_par->smartEnc <= 5) {
#if 0
            if (rcCfg.crf < 0)
                rcCfg.crf = 28 + video_par->smartEnc;
#endif
            rcCfg.crf = -1;//smartEnc not use capped crf.


            if (rcCfg.cpbMaxRate == 0) {
                rcCfg.cpbMaxRate = (double) 4000000 * video_par->width * video_par->height * rcCfg.frameRateNum / rcCfg.frameRateDenom / 1920 / 1080 / 30;
                rcCfg.cpbMaxRate *= IS_HEVC(ctx->codecFormat) ? 0.6 : 1;
                rcCfg.cpbMaxRate *= (1 + (3 - video_par->smartEnc) * 0.2);
            }

            rcCfg.bitPerSecond = rcCfg.cpbMaxRate;
            if (rcCfg.hrd && rcCfg.hrdCpbSize == 0)
                rcCfg.hrdCpbSize = rcCfg.cpbMaxRate * 2;
        }

        if ((encRet = VCEncSetRateCtrl(chn->codec_inst, &rcCfg)) != VCENC_OK) {
            LOG_ERROR(ENC, "VCEncSetRateCtrl() failed: %d", encRet);
            return vmpp_RSLT_ERR_ENC_INIT;
        }
    }
    return vmpp_RSLT_OK;
}

static vmppResult setupRateCtrl(struct video_encoder_private_context *ctx,
                                struct va_enc_channel *chn, encChannelParameters *param)
{
    VCEncRet encRet;
    VCEncRateCtrl rcCfg;
    encVideoConfiguration *video_par;
    memset(&rcCfg, 0, sizeof(VCEncRateCtrl));

    video_par = &param->videoConfig;

    if ((encRet = VCEncGetRateCtrl(chn->codec_inst, &rcCfg)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncGetRateCtrl() failed: %d", encRet);
        VCEncRelease(chn->codec_inst);
        chn->codec_inst = NULL;
        return vmpp_RSLT_ERR_ENC_INIT;
    } else {
        LOG_INFO(ENC,
                 "Get rate control: qp %2d qpRange I[%2d, %2d] PB[%2d, %2d] %8d bps  "
                 "pic %d skip %d  hrd %d  vbvBufSize %d vbvMaxRate %d bitrateWindow %d "
                 "intraQpDelta %2d",
                 rcCfg.qpHdr, rcCfg.qpMinI, rcCfg.qpMaxI, rcCfg.qpMinPB, rcCfg.qpMaxPB,
                 rcCfg.bitPerSecond, rcCfg.pictureRc, rcCfg.pictureSkip, rcCfg.hrd,
                 rcCfg.hrdCpbSize, rcCfg.cpbMaxRate, rcCfg.bitrateWindow, rcCfg.intraQpDelta);

        rcCfg.qpHdr = -1;
        rcCfg.pictureSkip = video_par->pictureSkip /*param->videoConfig.rateControl.pictureSkip*/;
        rcCfg.pictureRc = 1 /*!cml->cqp*/; // canada add crf
        rcCfg.ctbRc = 0;
        rcCfg.ctbRcQpDeltaReverse = 0;
        rcCfg.blockRCSize = 0;
        rcCfg.rcQpDeltaRange = 15;
        rcCfg.rcBaseMBComplexity = 15;
        rcCfg.bitPerSecond = 1000000;
        rcCfg.cpbMaxRate = 0;
        rcCfg.hrdCpbSize = 0;
        rcCfg.bitVarRangeI = 10000;
        rcCfg.bitVarRangeP = 10000;
        rcCfg.bitVarRangeB = 10000;
        rcCfg.tolMovingBitRate = 2000;
        /* Same default as in the VSI reference test bench, the valid range is [50, 100].
         * It must not keep the value returned by VCEncGetRateCtrl() for a fresh instance
         * (-1): the rate control then derives a negative bit budget for the first picture
         * and aborts on an internal assertion. */
        rcCfg.changePos = 90;
        rcCfg.longTermQpDelta = 0;
        rcCfg.monitorFrames =
            (video_par->frameRate.numerator + video_par->frameRate.denominator - 1) /
            video_par->frameRate.denominator;
        if (rcCfg.monitorFrames > 120 /*MOVING_AVERAGE_FRAMES*/)
            rcCfg.monitorFrames = 120 /*MOVING_AVERAGE_FRAMES*/;
        if (rcCfg.monitorFrames < 10) {
            rcCfg.monitorFrames =
                (video_par->frameRate.numerator > video_par->frameRate.denominator)
                    ? 10
                    : 3 /*LEAST_MONITOR_FRAME*/;
        }
        rcCfg.hrd = video_par->hrd;
        rcCfg.bitrateWindow = 300;
        rcCfg.intraQpDelta = -2;
        rcCfg.vbr = 0;
        rcCfg.crf = -1;
        rcCfg.fixedIntraQp = 0;
        rcCfg.smoothPsnrInGOP = 0;
        rcCfg.u32StaticSceneIbitPercent = 80;
        rcCfg.qpMinI = rcCfg.qpMinPB = 12;
        if (video_par->cqp != VMPP_ENC_DEFAULT_PAR && video_par->cqp) { //not set qpMin=12 and intraQpDelta=-2 for cqp
            rcCfg.qpMinI = rcCfg.qpMinPB = 0;
            rcCfg.intraQpDelta = 0;
        }
        if (video_par->crf >= 0 && video_par->vbvBufSize == 0 && video_par->vbvMaxRate == 0)  //not set qpMin=12 for crf
            rcCfg.qpMinI = rcCfg.qpMinPB = 0;

        rcCfg.frameRateDenom = video_par->frameRate.denominator;
        rcCfg.frameRateNum = video_par->frameRate.numerator;

        if (video_par->initQp != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpHdr = video_par->initQp;

        if (video_par->qpMinPB != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpMinI = rcCfg.qpMinPB = video_par->qpMinPB;

        if (video_par->qpMinI != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpMinI = video_par->qpMinI;

        if (video_par->qpMaxPB != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpMaxI = rcCfg.qpMaxPB = video_par->qpMaxPB;

        if (video_par->qpMaxI != VMPP_ENC_DEFAULT_PAR)
            rcCfg.qpMaxI = video_par->qpMaxI;

        if (video_par->bitRate != VMPP_ENC_DEFAULT_PAR)
            rcCfg.bitPerSecond = video_par->bitRate;
        else
            rcCfg.bitPerSecond = 1000000;

        if (video_par->intraQpDelta != (int32_t)VMPP_ENC_DEFAULT_PAR)
            rcCfg.intraQpDelta = video_par->intraQpDelta;
        else if ((video_par->keyInt == 0 || video_par->keyInt >= 50) &&
                 (video_par->gopSize <= 2 || video_par->gopSize == 4 ||
                  video_par->gopSize == 8))
            rcCfg.intraQpDelta = -2;
        else
            rcCfg.intraQpDelta = 0;

        if (video_par->vbvBufSize != VMPP_ENC_DEFAULT_PAR)
            rcCfg.hrdCpbSize = video_par->vbvBufSize;
        if (video_par->vbvMaxRate != VMPP_ENC_DEFAULT_PAR)
            rcCfg.cpbMaxRate = video_par->vbvMaxRate;

        if (video_par->cqp != VMPP_ENC_DEFAULT_PAR) {
            rcCfg.pictureRc = !video_par->cqp;
            if (video_par->cqp) {
            }
        }

        if (video_par->crf >= 0) {
            rcCfg.crf = video_par->crf;
            rcCfg.pictureRc = 0;
        }

        if (video_par->lookaheadDepth == 0 && video_par->crf >= 0) {
            if (rcCfg.frameRateDenom != 0)
                rcCfg.qpHdr =
                    rcCfg.crf + (uint32_t)(2.9 + 2.4 * log2f((float)rcCfg.frameRateNum /
                                                             (rcCfg.frameRateDenom * 25)));
            else
                rcCfg.qpHdr = -1;
            rcCfg.qpHdr = CLIP3(0, 51, rcCfg.qpHdr);

            if (video_par->profile == vmpp_VIDEO_PRFL_HEVC_MAIN_STILL_PICTURE)
                rcCfg.qpHdr = CLIP3(0, 51, rcCfg.crf + 2);
        }

        if (video_par->llRc != VMPP_ENC_DEFAULT_PAR) {
            switch (video_par->llRc) {
            case 0:
                rcCfg.ctbRc = 0;
                rcCfg.bitrateWindow = 300;
                break;
            case 1:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 5.0;
                rcCfg.tolCtbRcIntra = 5.0;
                rcCfg.bitrateWindow = 20;
                break;
            case 2:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 3.0;
                rcCfg.tolCtbRcIntra = 3.0;
                rcCfg.bitrateWindow = 10;
                break;
            case 3:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 2.0;
                rcCfg.tolCtbRcIntra = 2.0;
                rcCfg.bitrateWindow = 5;
                break;
            case 4:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 1.0;
                rcCfg.tolCtbRcIntra = 1.0;
                rcCfg.bitrateWindow = 5;
                break;
            case 5:
                rcCfg.ctbRc = 2;
                rcCfg.tolCtbRcInter = 0.5;
                rcCfg.tolCtbRcIntra = 0.5;
                rcCfg.bitrateWindow = 5;
                break;
            default:
                rcCfg.ctbRc = 0;
            }
            rcCfg.ctbRc += (video_par->lookaheadDepth == 0 && (video_par->tune == 1 || video_par->tune == 2 || video_par->tune == 3));
        }

#if 0
        if (param->videoConfig.rcMode == vmpp_RC_CBR) {
            rcCfg.pictureRc = 1;
            rcCfg.vbr = 0;
            rcCfg->bitrateWindow = 50;
        } else if (param->videoConfig.rcMode == vmpp_RC_VBR) {
            rcCfg.pictureRc = 1;
            rcCfg->vbr = 1;
            rcCfg.hrd = 0;
            rcCfg.hrdCpbSize = 0;
            rcCfg->bitrateWindow = 300;
        } else if (param->videoConfig.rcMode == vmpp_RC_CQP) {
            rcCfg.pictureRc = 1;
            if(rcCfg.crf >= 0)//canada add crf
                rcCfg->pictureRc = 0;
            rcCfg->ctbRc = 0;
            rcCfg->bitrateWindow = 150;
        }
#endif

#ifdef ENABLE_MFSM
        rcCfg.maxFrameSizeMultiple = 0.0;
        if (video_par->maxFrameSizeMultiple != VMPP_ENC_DEFAULT_PAR)
            rcCfg.maxFrameSizeMultiple = video_par->maxFrameSizeMultiple;
#endif

        if(rcCfg.ctbRc & 2)
        {
            if(video_par->maxFrameSize > 0)
            {
            }
            else
            {
                // default value is (width * height * 1.5 * 8 / 32) bit
            }
        }
        else
        {
        }

        if (video_par->smartEnc > 0 && video_par->smartEnc <= 5) {
#if 0
            if (rcCfg.crf < 0)
                rcCfg.crf = 28 + video_par->smartEnc;
#endif
            rcCfg.crf = -1;//smartEnc not use capped crf.

            if (rcCfg.cpbMaxRate == 0) {
                rcCfg.cpbMaxRate = (double) 4000000 * video_par->width * video_par->height * rcCfg.frameRateNum / rcCfg.frameRateDenom / 1920 / 1080 / 30;
                rcCfg.cpbMaxRate *= (IS_HEVC(ctx->codecFormat) ? 0.6 : 1);
                rcCfg.cpbMaxRate *= (1 + (3 - video_par->smartEnc) * 0.2);
            }

            rcCfg.bitPerSecond = rcCfg.cpbMaxRate;
            if( rcCfg.hrd && rcCfg.hrdCpbSize == 0)
                rcCfg.hrdCpbSize = rcCfg.cpbMaxRate * 2;
        }

        /* The bit rate balance level and the I frame QP factor are not
           supported by this library version. */
        if (video_par->hrd && video_par->crf >= 0)
            rcCfg.hrd = 0;

        LOG_INFO(ENC,
                 "Set rate control: qp %2d qpRange I[%2d, %2d] PB[%2d, %2d] %8d bps  "
                 "pic %d skip %d  hrd %d ctbrc %d"
                 "  vbvBufSize %d vbvMaxRate %d bitrateWindow %d intraQpDelta %2d "
                 "fixedIntraQp %2d frameRateNum %d frameRateDenom %d crf %d ",
                 rcCfg.qpHdr, rcCfg.qpMinI, rcCfg.qpMaxI, rcCfg.qpMinPB, rcCfg.qpMaxPB,
                 rcCfg.bitPerSecond, rcCfg.pictureRc, rcCfg.pictureSkip, rcCfg.hrd, rcCfg.ctbRc,
                 rcCfg.hrdCpbSize, rcCfg.cpbMaxRate, rcCfg.bitrateWindow, rcCfg.intraQpDelta,
                 rcCfg.fixedIntraQp, rcCfg.frameRateNum, rcCfg.frameRateDenom, rcCfg.crf);

        if ((encRet = VCEncSetRateCtrl(chn->codec_inst, &rcCfg)) != VCENC_OK) {
            LOG_ERROR(ENC, "VCEncSetRateCtrl() failed: %d", encRet);
            VCEncRelease(chn->codec_inst);
            chn->codec_inst = NULL;
            return vmpp_RSLT_ERR_ENC_INIT;
        }
        memcpy(&ctx->rcCfg, &rcCfg, sizeof(rcCfg));
    }

    return vmpp_RSLT_OK;
}

static vmppResult setupPreProc(struct video_encoder_private_context *ctx,
                               struct va_enc_channel *chn, encChannelParameters *param,
                               const vmppFrame *frame)
{
    VCEncRet encRet;
    VCEncPreProcessingCfg preProcCfg;
    memset(&preProcCfg, 0, sizeof(VCEncPreProcessingCfg));

    //UNUSED_PARAMETER(param);
    encVideoConfiguration *video_par;
    video_par = &param->videoConfig;
    if ((encRet = VCEncGetPreProcessing(chn->codec_inst, &preProcCfg)) != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncGetPreProcessing() failed: %d", encRet);
        VCEncRelease(chn->codec_inst);
        chn->codec_inst = NULL;
        return vmpp_RSLT_ERR_ENC_INIT;
    } else {
        if (frame->memoryType == vmpp_MEM_DEVICE && frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010)
            preProcCfg.origWidth = frame->stride[0] / 2;
        else
            preProcCfg.origWidth = frame->stride[0];

        preProcCfg.origHeight = frame->height;
        /* The crop information of the SPS is not supported by this library
           version, only the cropping of the input picture is applied. */
        if (frame->cropInfo.flag == vmpp_CROP_ENABLE) {
            preProcCfg.xOffset = frame->cropInfo.xOffset;
            preProcCfg.yOffset = frame->cropInfo.yOffset;
        }
        preProcCfg.inputType = picformatPar2Internal(frame->pixelFormat);
        preProcCfg.rotation = (VCEncPictureRotation)video_par->rotation;
        preProcCfg.mirror = VCENC_MIRROR_NO;
        preProcCfg.videoStabilization = 0;
        preProcCfg.colorConversion.type = VCENC_RGBTOYUV_BT601;
        if (preProcCfg.colorConversion.type == VCENC_RGBTOYUV_USER_DEFINED) {
            preProcCfg.colorConversion.coeffA = 20000;
            preProcCfg.colorConversion.coeffB = 44000;
            preProcCfg.colorConversion.coeffC = 5000;
            preProcCfg.colorConversion.coeffE = 35000;
            preProcCfg.colorConversion.coeffF = 38000;
            preProcCfg.colorConversion.coeffG = 35000;
            preProcCfg.colorConversion.coeffH = 38000;
            preProcCfg.colorConversion.LumaOffset = 0;
        }
        preProcCfg.scaledWidth = 0;
        preProcCfg.scaledHeight = 0;
        preProcCfg.busAddressScaledBuff = (vmppDevAddr)NULL;
        preProcCfg.virtualAddressScaledBuff = NULL;
        preProcCfg.sizeScaledBuff = 0;
        preProcCfg.input_alignment = 1 << ctx->cfg.exp_of_input_alignment;
        preProcCfg.scaledOutputFormat = 0;
        preProcCfg.constChromaEn = 0;
        /* Set overlay area*/
        for (int i = 0; i < MAX_OVERLAY_NUM; i++) {
            preProcCfg.overlayArea[i].xoffset = 0;
            preProcCfg.overlayArea[i].cropXoffset = 0;
            preProcCfg.overlayArea[i].yoffset = 0;
            preProcCfg.overlayArea[i].cropYoffset = 0;
            preProcCfg.overlayArea[i].width = 0;
            preProcCfg.overlayArea[i].cropWidth = 0;
            preProcCfg.overlayArea[i].height = 0;
            preProcCfg.overlayArea[i].cropHeight = 0;
            preProcCfg.overlayArea[i].format = 0;
            preProcCfg.overlayArea[i].alpha = 0;
            preProcCfg.overlayArea[i].enable = 0;
            preProcCfg.overlayArea[i].Ystride = 0;
            preProcCfg.overlayArea[i].UVstride = 0;
            preProcCfg.overlayArea[i].bitmapY = 0;
            preProcCfg.overlayArea[i].bitmapU = 0;
            preProcCfg.overlayArea[i].bitmapV = 0;
            preProcCfg.overlayArea[i].superTile = 0;
            preProcCfg.overlayArea[i].scaleWidth = 0;
            preProcCfg.overlayArea[i].scaleHeight = 0;
        }
        /* Set mosaic region parameters */
        for (int i = 0; i < MAX_MOSAIC_NUM; i++) {
            preProcCfg.mosEnable[i] = 0;
            preProcCfg.mosXoffset[i] = 0;
            preProcCfg.mosYoffset[i] = 0;
            preProcCfg.mosWidth[i] = 0;
            preProcCfg.mosHeight[i] = 0;
        }

        if ((encRet = VCEncSetPreProcessing(chn->codec_inst, &preProcCfg)) != VCENC_OK) {
            LOG_ERROR(ENC, "VCEncSetPreProcessing() failed: %d", encRet);
            VCEncRelease(chn->codec_inst);
            chn->codec_inst = NULL;
            return vmpp_RSLT_ERR_ENC_INIT;
        }
    }

    memcpy(&ctx->preProcCfg, &preProcCfg, sizeof(preProcCfg));

    return vmpp_RSLT_OK;
}

static void applyGopConfig(struct video_encoder_private_context *ctx, encChannelParameters *param)
{
    if (ctx->workmode == MULTI_CORE_MODE)
    {
        ctx->gopPicCfg = ctx->gopPicCfg_tmp;
        ctx->gopPicCfgPass2 =ctx->gopPicCfgPass2_tmp;
        ctx->gopPicSpecialCfg =ctx->gopPicSpecialCfg_tmp;
    }
    else
    {
        ctx->gopPicCfg = (VCEncGopPicConfig *)((void *)ctx->encIn + ALLIGN(sizeof(VCEncIn), 16) +
                                            ALLIGN(sizeof(VCEncOut), 16));
        ctx->gopPicCfgPass2 =
            (VCEncGopPicConfig *)((void *)ctx->encIn + ALLIGN(sizeof(VCEncIn), 16) +
                                ALLIGN(sizeof(VCEncOut), 16) +
                                ALLIGN(sizeof(VCEncGopPicConfig), 16) * MAX_GOP_PIC_CONFIG_NUM);
        ctx->gopPicSpecialCfg =
            (VCEncGopPicSpecialConfig *)((void *)ctx->encIn + ALLIGN(sizeof(VCEncIn), 16) +
                                        ALLIGN(sizeof(VCEncOut), 16) +
                                        ALLIGN(sizeof(VCEncGopPicConfig), 16) *
                                            MAX_GOP_PIC_CONFIG_NUM +
                                        ALLIGN(sizeof(VCEncGopPicConfig), 16) *
                                            MAX_GOP_PIC_CONFIG_NUM);
        memcpy(ctx->gopPicCfg, ctx->gopPicCfg_tmp, sizeof(VCEncGopPicConfig) * MAX_GOP_PIC_CONFIG_NUM);
        memcpy(ctx->gopPicCfgPass2, ctx->gopPicCfgPass2_tmp,
            sizeof(VCEncGopPicConfig) * MAX_GOP_PIC_CONFIG_NUM);
        memcpy(ctx->gopPicSpecialCfg, ctx->gopPicSpecialCfg_tmp,
            sizeof(VCEncGopPicSpecialConfig) * MAX_GOP_SPIC_CONFIG_NUM);
    }

    ctx->encIn->gopConfig.pGopPicCfg = ctx->gopPicCfg;
    ctx->encIn->gopConfig.pGopPicSpecialCfg = ctx->gopPicSpecialCfg;

    if (param->videoConfig.lookaheadDepth) {
        ctx->encIn->gopConfig.pGopPicSpecialCfg = ctx->gopPicSpecialCfg;
        ctx->encIn->gopConfig.pGopPicCfgPass1 = ctx->gopPicCfg;
        ctx->encIn->gopConfig.pGopPicCfg = ctx->encIn->gopConfig.pGopPicCfgPass2 =
            ctx->gopPicCfgPass2;
    }

    ctx->encIn->gopConfig.idr_interval = param->videoConfig.keyInt /*tb.idr_interval*/;
    ctx->gdrDuration = param->videoConfig.gdrDuration;
    ctx->encIn->gopConfig.firstPic = 0;
    ctx->encIn->gopConfig.lastPic = ctx->vFrames;

    ctx->encIn->gopConfig.outputRateNumer = ctx->cfg.frameRateNum;
    ctx->encIn->gopConfig.outputRateDenom = ctx->cfg.frameRateDenom;
    ctx->encIn->gopConfig.inputRateNumer = ctx->cfg.frameRateNum;
    ctx->encIn->gopConfig.inputRateDenom = ctx->cfg.frameRateDenom;
    ctx->encIn->gopConfig.gopLowdelay = ctx->gopLowdelay;
    ctx->encIn->gopConfig.interlacedFrame = 0;
}

/* Copies the planes of a host frame into the hardware visible input buffer. */
static vmppResult copyInputPicture(vmppFrame *frame,
                                   EWLLinearMem_t *inputBuf, uint32_t lumaSize, uint32_t chromaSize)
{
    int planes;
    int bytes_per_pixel = 1;
    uint8_t *dst = (uint8_t *)inputBuf->virtualAddress;
    uint8_t *dstBase;

    if (frame->pixelFormat == vmpp_PIX_FMT_YUV420P ||
        frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE) {
        planes = 3;
    } else if (frame->pixelFormat == vmpp_PIX_FMT_RGBA) {
        planes = 1;
    } else {
        planes = 2;
    }

    if (frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE ||
        frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_P010) {
        bytes_per_pixel = 2;
    }
    if (frame->pixelFormat == vmpp_PIX_FMT_RGBA) {
        bytes_per_pixel = 4;
    }

    if (dst == NULL || frame->data[0] == NULL)
        return vmpp_RSLT_ERR_INVALID_PARAMS;

    for (int i = 0; i < planes; i++) {
        int w = (int)frame->width;
        int h = (int)frame->height;
        uint32_t srcStride = frame->stride[0];
        uint32_t desStride = lumaSize / frame->height;

        if (i == 0) {
            dstBase = dst;
        } else if (frame->pixelFormat == vmpp_PIX_FMT_YUV420P ||
                   frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE) {
            w /= 2;
            srcStride /= 2;
            desStride = chromaSize / frame->height;
            dstBase = dst + lumaSize + (i - 1) * (chromaSize / 2);
            h /= 2;
        } else {
            desStride = chromaSize / (frame->height / 2);
            dstBase = dst + lumaSize;
            h /= 2;
        }

        if (frame->data[i] == NULL)
            break;

        for (int row = 0; row < h; row++)
            memcpy(dstBase + row * desStride, frame->data[i] + row * srcStride,
                   (size_t)w * bytes_per_pixel);
    }

    (void)EWLSyncMemData(inputBuf, 0, lumaSize + chromaSize, HOST_TO_DEVICE);
    return vmpp_RSLT_OK;
}

/* Makes the hardware output of a stream buffer visible for the CPU. */
static int DMATransRead(EWLLinearMem_t *buf, uint32_t offset, uint32_t size, u8 *dma_vir_buf)
{
    UNUSED_PARAMETER(dma_vir_buf);
    UNUSED_PARAMETER(offset);
    UNUSED_PARAMETER(size);
    if (buf == NULL)
        return -1;

    return (int)EWLSyncMemData(buf, offset, size, DEVICE_TO_HOST);
}

static vmppResult allocExtSramRes(struct va_enc_channel *chn,
                                  struct video_encoder_private_context *ctx)
{
    int ret = 0;
    const void *ewl_inst = ctx->ewlInst;
    const EWLHwConfig_t *asicCfg = EncGetAsicConfig(ctx->codecFormat, ewl_inst);
    UNUSED_PARAMETER(chn);
    uint32_t extSramWidthAlignment = 0;
    uint32_t extSramTotal = 0;
    if (IS_HEVC(ctx->codecFormat) && ctx->cfg.bitDepthLuma == 8) // hevc main8
        extSramWidthAlignment = 8;
    else if (IS_HEVC(ctx->codecFormat) && ctx->cfg.bitDepthLuma == 10) // hevc main10
        extSramWidthAlignment = 16;
    else if (IS_H264(ctx->codecFormat)) // h264
        extSramWidthAlignment = 16;

    uint32_t stride = STRIDE(ctx->cfg.width, extSramWidthAlignment);
    ctx->extSramLumBwdSize =
        ctx->cfg.extSramLumHeightBwd * 4 * stride * 10 / (ctx->cfg.bitDepthLuma == 10 ? 8 : 10);
    ctx->extSramLumFwdSize =
        ctx->cfg.extSramLumHeightFwd * 4 * stride * 10 / (ctx->cfg.bitDepthLuma == 10 ? 8 : 10);
    ctx->extSramChrBwdSize =
        ctx->cfg.extSramChrHeightBwd * 4 * stride * 10 / (ctx->cfg.bitDepthChroma == 10 ? 8 : 10);
    ctx->extSramChrFwdSize =
        ctx->cfg.extSramChrHeightFwd * 4 * stride * 10 / (ctx->cfg.bitDepthChroma == 10 ? 8 : 10);

    if (asicCfg != NULL && asicCfg->meExternSramSupport)
        extSramTotal = ctx->extSramLumBwdSize + ctx->extSramLumFwdSize + ctx->extSramChrBwdSize +
                       ctx->extSramChrFwdSize;

    for (int coreIdx = 0; coreIdx < ctx->parallelCoreNum; coreIdx++) {
        if (extSramTotal != 0) {
            ctx->extSRAMMemFactory[coreIdx].mem_type = EWL_MEM_TYPE_VPU_WORKING;
            ret = EWLMallocLinear(ewl_inst, extSramTotal, 16, &ctx->extSRAMMemFactory[coreIdx]);
            if (ret != EWL_OK) {
                ctx->extSRAMMemFactory[coreIdx].virtualAddress = NULL;
                return vmpp_RSLT_ERR_ENC_EWL;
            }

            dmaMemZero(&ctx->extSRAMMemFactory[coreIdx], extSramTotal);
        }
    }
    return vmpp_RSLT_OK;
}

#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
static vmppResult allocROIMapBuffer(struct va_enc_channel *chn,
                                    struct video_encoder_private_context *ctx,
                                    const vmppFrame *frame)
{
    int32_t max_cu_size = 64;
    if (IS_H264(ctx->codecFormat))
        max_cu_size = 16;
    uint32_t block_size = ((frame->width + max_cu_size - 1) & (~(max_cu_size - 1))) *
                          ((frame->height + max_cu_size - 1) & (~(max_cu_size - 1))) / (8 * 8 * 2);
    if (ctx->roiMapVersion >= 1)
        block_size *= 2;
    block_size = ((block_size + 63) & (~63));
    const void *ewl_inst = ctx->ewlInst;
    ctx->roiMapDeltaQpMemFactory[0].mem_type = EWL_MEM_TYPE_VPU_WORKING;
    if (EWLMallocLinear(ewl_inst, block_size * ctx->bufferCnt + ROIMAP_PREFETCH_EXT_SIZE, 0,
                        &ctx->roiMapDeltaQpMemFactory[0]) != EWL_OK) {
        ctx->roiMapDeltaQpMemFactory[0].virtualAddress = NULL;
        return vmpp_RSLT_ERR_ENC_EWL;
    }

    int DieIndex = EWLGetDieIndex(ewl_inst);
    /* Currently, the ROI map Delta QP Memory is only needed for HEVC 2 pass, there is no need for
     * the virtual address */
#if VMPP_ENABLE_ROI_MAP
    ctx->roiMapDeltaQpMemFactory[0].virtualAddress =
        dmaMemGetSet(chn->codec_inst, DieIndex, block_size * ctx->bufferCnt + ROIMAP_PREFETCH_EXT_SIZE,
                     ctx->roiMapDeltaQpMemFactory[0].busAddress);
#else
    dmaMemGetSetEx(chn->codec_inst, DieIndex, block_size * ctx->bufferCnt + ROIMAP_PREFETCH_EXT_SIZE,
                   ctx->roiMapDeltaQpMemFactory[0].busAddress);
#endif
    i32 total_size = ctx->roiMapDeltaQpMemFactory[0].size;
    for (int coreIdx = 0; coreIdx < ctx->bufferCnt; coreIdx++) {
#if VMPP_ENABLE_ROI_MAP
        ctx->roiMapDeltaQpMemFactory[coreIdx].virtualAddress =
            (uint32_t *)((vmppAddr)ctx->roiMapDeltaQpMemFactory[0].virtualAddress +
                         coreIdx * block_size);
        memset(ctx->roiMapDeltaQpMemFactory[coreIdx].virtualAddress, 0, block_size);
#endif
        ctx->roiMapDeltaQpMemFactory[coreIdx].busAddress =
            ctx->roiMapDeltaQpMemFactory[0].busAddress + coreIdx * block_size;
        ctx->roiMapDeltaQpMemFactory[coreIdx].size =
            (coreIdx < ctx->bufferCnt - 1 ? block_size
                                          : total_size - (ctx->bufferCnt - 1) * block_size);
    }

    return vmpp_RSLT_OK;
}
#endif

/* In single core mode applyGopConfig() lays out the GOP configuration arrays directly behind
 * the VCEncIn structure (reserving room for VCEncOut as well), so the buffer holding
 * ctx->encIn must be large enough to contain all of them. */
static size_t encInBufferSize(void)
{
    return ALLIGN(sizeof(VCEncIn), 16) + ALLIGN(sizeof(VCEncOut), 16) +
           ALLIGN(sizeof(VCEncGopPicConfig), 16) * MAX_GOP_PIC_CONFIG_NUM * 2 +
           ALLIGN(sizeof(VCEncGopPicSpecialConfig), 16) * MAX_GOP_SPIC_CONFIG_NUM;
}

vmppResult video_encoder_initialize_chn(struct va_enc_channel *chn, encChannelParameters *param,
                                        const vmppFrame *frame)
{
    vmppResult ret;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;

    ret = setupCodingConfig(ctx, chn, frame);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ret = setupRateCtrl(ctx, chn, param);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ret = setupPreProc(ctx, chn, param, frame);
    if (ret != vmpp_RSLT_OK)
        return ret;

    applyGopConfig(ctx, param);

    ctx->streamBufNum = 1;
    ctx->frameDelay = ctx->parallelCoreNum > 1 ? 4 : 1;
    ctx->bufferCnt = ctx->frameDelay + (param->videoConfig.gopSize == 16 ? 16 : VMPP_BUFFER_CNT_FOR_REORDER);
    ctx->pictureEncCount = 0;
    if (param->videoConfig.lookaheadDepth) {
        int32_t delay =
            CUTREE_BUFFER_CNT(param->videoConfig.lookaheadDepth, param->videoConfig.gopSize) -
            1; // delay for cutree input queue
        ctx->frameDelay = VA_MIN(ctx->vFrames /*- cml->firstPic + 2*/, delay + ctx->frameDelay);
        /* consider gop8->gop4 reorder: 8 4 2 1 3 6 5 7 -> 4 2 1 3 8 6 5 7
         * at least 4 more buffers are needed to avoid buffer overwrite in pass1 before consumed in
         * pass2*/
        ctx->bufferCnt += ctx->frameDelay;
    }
    LOG_INFO(ENC, "input buffer number: %d", ctx->bufferCnt);

    ret = allocExtSramRes(chn, ctx);
    if (ret != vmpp_RSLT_OK)
        return ret;
#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
    if (chn->params.videoConfig.lookaheadDepth && IS_HEVC(ctx->codecFormat)) {
        LOG_INFO(ENC, "Is running 2 pass encoding for HEVC, alloc roi map buffer");
        ret = allocROIMapBuffer(chn, ctx, frame);
        if (ret != vmpp_RSLT_OK)
            return ret;
    }
#endif

    InitPicConfig(ctx->encIn, chn);
    ctx->nextGopSize = ctx->encIn->gopSize;

    if (param->outbufNum == 0)
        chn->outbufNum = VA_ENC_DEF_OUTPUT_BUFFER;
    else if (param->outbufNum > VA_ENC_MAX_OUTPUT_BUFFER)
        chn->outbufNum = VA_ENC_MAX_OUTPUT_BUFFER;
    else
        chn->outbufNum = param->outbufNum;

    // Adaptive Gop variables
    ctx->agop.last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
    ctx->agop.gop_frm_num = 0;
    ctx->agop.sum_intra_vs_interskip = 0;
    ctx->agop.sum_skip_vs_interskip = 0;
    ctx->agop.sum_intra_vs_interskipP = 0;
    ctx->agop.sum_intra_vs_interskipB = 0;
    ctx->agop.sum_costP = 0;
    ctx->agop.sum_costB = 0;

    ctx->parametersSetOutputed = 0;
    ctx->ivfHeaderOutputed = 0;
    ctx->eos = 0;
    ctx->lastVRet = VCENC_OK;
    return vmpp_RSLT_OK;
}

vmppResult video_encoder_create_chn(struct va_enc_channel *chn, encChannelParameters *param)
{

    if (!param || !chn) {
        LOG_ERROR(ENC, "Invalid parameters: param %p, chn %p", param, chn);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    vmppResult ret;

    struct video_encoder_private_context *ctx =
        malloc(sizeof(struct video_encoder_private_context));
    if (!ctx) {
        LOG_ERROR(ENC, "Fail to malloc private context for video encoder.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }
    memset(ctx, 0, sizeof(struct video_encoder_private_context));
    chn->private_context = ctx;
    ctx->codecFormat = formatPar2Internal(param->codecType);

    ctx->roiMapDeltaQpEnable = param->videoConfig.roiType == vmpp_ENC_ROI_MAP;
    ctx->roiMapDeltaQpBlockUnit = param->videoConfig.roiMapDeltaQpBlockUnit;
    ctx->sliceSize = param->videoConfig.sliceSize;

    /* The buffers of the channel are allocated through a private EWL instance.
     * It has to be created before checkParameters(), because the HW capability
     * query (EncGetAsicConfig()) needs a valid EWL instance. */
    ctx->workmode = SINGLE_CORE_MODE;
    {
        EWLInitParam_t ewlParam;
        memset(&ewlParam, 0, sizeof(ewlParam));
        ewlParam.clientType = IS_H264(ctx->codecFormat) ? EWL_CLIENT_TYPE_H264_ENC
                                                        : EWL_CLIENT_TYPE_HEVC_ENC;

        ewlParam.slice_idx = 0;
        ewlParam.enc_dev = param->encDevice;// "/dev/hantroenc"; /** \brief The device name of the enc driver. */
        ewlParam.mem_dev = param->memDevice;// "/dev/memalloc";  /** \brief The device name of the memalloc driver. */
        ewlParam.useVcmd = 0;  /** \brief Specifies whether to use VCMD, only valid for cmodel. \n 0: Not use. \n 1: Use. */
        ctx->ewlInst = EWLInit(&ewlParam);
        if (ctx->ewlInst == NULL) {
            LOG_ERROR(ENC, "EWLInit() failed.");
            return vmpp_RSLT_ERR_NO_MEMORY;
        }
    }

    ret = checkParameters(ctx, chn, param);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ctx->roiType = param->videoConfig.roiType;
    ctx->vFrames = 0x7FFFFFFF;
    ctx->longterm_enable = 0;

    ctx->encIn = (VCEncIn *)calloc(1, encInBufferSize());
    if (!ctx->encIn) {
        LOG_ERROR(ENC, "Fail to malloc VCEncIn.");
        return vmpp_RSLT_ERR_NO_MEMORY;
    }

    ret = setupGop(ctx, chn, param);
    if (ret != vmpp_RSLT_OK) {
        free(ctx->encIn);
        ctx->encIn = NULL;
        return ret;
    }

    ret = initEncoder(ctx, chn, param);
    if (ret != vmpp_RSLT_OK) {
        free(ctx->encIn);
        ctx->encIn = NULL;
        return ret;
    }

    ctx->parallelCoreNum = ctx->cfg.parallelCoreNum;
    {
        ctx->encOut = (VCEncOut *)calloc(1, sizeof(VCEncOut));
        if (!ctx->encOut) {
            LOG_ERROR(ENC, "Fail to malloc VCEncOut.");
            return vmpp_RSLT_ERR_NO_MEMORY;
        }
    }
    return vmpp_RSLT_OK;
}

vmppResult video_encoder_destory_chn(struct va_enc_channel *chn)
{
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    if (ctx) {
        freeRes(chn);
        if (!ctx->hashLenMismatch && chn->codec_inst) {
            VCEncRelease((VCEncInst)chn->codec_inst);
            chn->codec_inst = NULL;
        }
        if (ctx->parametersSet)
            free(ctx->parametersSet);

        if (ctx->ivfHeader)
            free(ctx->ivfHeader);

        freeAv1HeaderOutputBuffer(ctx);
#if 0
        LOG_INFO(ENC, "Average PSNR for %d frame: Y %4.2f, U %4.2f, V %4.2f", ctx->outputPictureCount,
                 ctx->psnr_total[0] / ctx->outputPictureCount,
                 ctx->psnr_total[1] / ctx->outputPictureCount,
                 ctx->psnr_total[2] / ctx->outputPictureCount);
#endif
        if (ctx->encIn)
            free(ctx->encIn);
        if (ctx->encOut)
            free(ctx->encOut);

        if (ctx->ewlInst) {
            EWLRelease(ctx->ewlInst);
            ctx->ewlInst = NULL;
        }

        free(ctx);
        chn->private_context = NULL;
    }

    clear_out_buffer_list(chn);

    return vmpp_RSLT_OK;
}

vmppResult video_encoder_alloc_frame(struct va_enc_channel *chn, vmppFrame *frame) {
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    UNUSED_PARAMETER(frame);
    if (!ctx) {
        LOG_ERROR(ENC, "Invalid parameters: ctx %p", ctx);
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
//  alloc memeory for frame

    return vmpp_RSLT_OK;
}


vmppResult video_encoder_free_frame(struct va_enc_channel *chn, vmppFrame *frame) {
    if (!chn || !frame)
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    if (frame->memoryType == vmpp_MEM_HOST && frame->data[0]) {
        free(frame->data[0]);
        frame->data[0] = NULL;
    }
    return vmpp_RSLT_OK;
}

static inline int isROIChanged(vmppEncPictureROI currentROI[VMPP_ENC_MAX_ROI_NUM],
                               vmppEncPictureROI lastROI[VMPP_ENC_MAX_ROI_NUM])
{
    return (memcmp(currentROI, lastROI, sizeof(vmppEncPictureROI) * VMPP_ENC_MAX_ROI_NUM) != 0);
}

static vmppResult recreateEncoder(struct va_enc_channel *chn, encChannelParameters *param,
                                  const vmppFrame *frame)
{
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    const void *ewl_inst = NULL;
    vmppResult ret = vmpp_RSLT_OK;
    if (chn->codec_inst)
        ewl_inst = ctx->ewlInst;

    for (int i = 0; i < MAX_CORE_NUM; i++) {
        if (ctx->extSRAMMemFactory[i].busAddress != (vmppDevAddr)NULL)
            EWLFreeLinear(ewl_inst, &ctx->extSRAMMemFactory[i]);
    }
#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
    if (ctx->roiMapDeltaQpMemFactory[0].busAddress != (vmppDevAddr)NULL) {
        for (int coreIdx = 1; coreIdx < ctx->bufferCnt; coreIdx++)
            ctx->roiMapDeltaQpMemFactory[0].size += ctx->roiMapDeltaQpMemFactory[coreIdx].size;
        EWLFreeLinear(ewl_inst, &ctx->roiMapDeltaQpMemFactory[0]);
    }
#endif
    // freeEWLRes(ctx, ewl_inst);
    // VCEncRelease((VCEncInst)chn->codec_inst);

    ctx->gopLowdelay = 0;

    param->videoConfig.width = frame->width;
    param->videoConfig.height = frame->height;

    if (ctx->parametersSet)
        free(ctx->parametersSet);
    ctx->parametersSet = NULL;

    if (ctx->ivfHeader)
        free(ctx->ivfHeader);
    ctx->ivfHeader = NULL;

    LOG_INFO(ENC, "Average PSNR: Y %4.2f, U %4.2f, V %4.2f",
             ctx->psnr_total[0] / ctx->outputPictureCount,
             ctx->psnr_total[1] / ctx->outputPictureCount,
             ctx->psnr_total[2] / ctx->outputPictureCount);

//    u32 workmode = EWLGetWorkMode(param->device);
//    ctx->workmode = workmode;

//    if (workmode == SINGLE_CORE_MODE)
    {
        /* The whole block is replaced here, release the old one first. The GOP config
         * pointers that used to point into it are re-established by applyGopConfig(). */
        VCEncIn *oldEncIn = ctx->encIn;
        ctx->encIn = NULL;
        free(oldEncIn);

        ctx->encIn = (VCEncIn *)calloc(1, encInBufferSize());
        if (!ctx->encIn) {
            LOG_ERROR(ENC, "Fail to malloc VCEncIn.");
            return vmpp_RSLT_ERR_NO_MEMORY;
        }
    }

    // ret = checkParameters(ctx, chn, param);
    // if (ret != vmpp_RSLT_OK)
    //     return ret;

    ret = setupGop(ctx, chn, param);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ret = reinitEncoder(ctx, chn, param);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ctx->parallelCoreNum = ctx->cfg.parallelCoreNum;
    /* The MCU parameter memory is not provided by this library version, the
       encoder input and output structures are allocated by the driver. */
    memset(ctx->encIn, 0, sizeof(VCEncIn));
    memset(ctx->encOut, 0, sizeof(VCEncOut));

    ret = setupCodingConfig(ctx, chn, frame);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ret = setupRateCtrl(ctx, chn, param);
    if (ret != vmpp_RSLT_OK)
        return ret;

    ret = setupPreProc(ctx, chn, param, frame);
    if (ret != vmpp_RSLT_OK)
        return ret;

    applyGopConfig(ctx, param);

    // ctx->parallelCoreNum = 1;
    // ctx->streamBufNum = 1;
    // ctx->frameDelay = 1;
    // ctx->bufferCnt = 1 + VMPP_BUFFER_CNT_FOR_REORDER;
    // ctx->pictureEncCount = 0;
    // if (param->videoConfig.lookaheadDepth) {
    //     int32_t delay =
    //         CUTREE_BUFFER_CNT(param->videoConfig.lookaheadDepth, param->videoConfig.gopSize) -
    //         1; // delay for cutree input queue
    //     ctx->frameDelay = VA_MIN(ctx->vFrames /*- cml->firstPic + 2*/, delay + ctx->frameDelay);
    //     /* consider gop8->gop4 reorder: 8 4 2 1 3 6 5 7 -> 4 2 1 3 8 6 5 7
    //      * at least 4 more buffers are needed to avoid buffer overwrite in pass1 before consumed
    //      in
    //      * pass2*/
    //     ctx->bufferCnt += ctx->frameDelay;
    // }
    LOG_INFO(ENC, "input buffer number: %d", ctx->bufferCnt);

    ret = allocExtSramRes(chn, ctx);
    if (ret != vmpp_RSLT_OK)
        return ret;

#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
    if (chn->params.videoConfig.lookaheadDepth && IS_HEVC(ctx->codecFormat)) {
        LOG_INFO(ENC, "Is running 2 pass encoding for HEVC, alloc roi map buffer");
        ret = allocROIMapBuffer(chn, ctx, frame);
        if (ret != vmpp_RSLT_OK)
            return ret;
    }
#endif

    InitPicConfig(ctx->encIn, chn);
    ctx->nextGopSize = ctx->encIn->gopSize;

    // if (param->outbufNum == 0)
    //     chn->outbufNum = VA_ENC_DEF_OUTPUT_BUFFER;
    // else if (param->outbufNum > VA_ENC_MAX_OUTPUT_BUFFER)
    //     chn->outbufNum = VA_ENC_MAX_OUTPUT_BUFFER;
    // else
    //     chn->outbufNum = param->outbufNum;

    // Adaptive Gop variables
    ctx->agop.last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
    ctx->agop.gop_frm_num = 0;
    ctx->agop.sum_intra_vs_interskip = 0;
    ctx->agop.sum_skip_vs_interskip = 0;
    ctx->agop.sum_intra_vs_interskipP = 0;
    ctx->agop.sum_intra_vs_interskipB = 0;
    ctx->agop.sum_costP = 0;
    ctx->agop.sum_costB = 0;

    ctx->parametersSetOutputed = 0;
    ctx->ivfHeaderOutputed = 0;
    ctx->eos = 0;
    ctx->lastVRet = VCENC_OK;

    return ret;
}

static void writeIvfHeader(u8 *ivfHeader, i32 width, i32 height, i32 rateNum, i32 rateDenom, i32 vp9)
{
	u8 data[32] = {0}; //Ivf stream headre
	/*IVF file header signature*/
    data[0] = 'D';
    data[1] = 'K';
    data[2] = 'I';
    data[3] = 'F';

    /*Header size*/
    data[6] = 32;

    /*Codec ForrCC*/
	if(!vp9){
	  data[8] = 'A';
      data[9] = 'V';
      data[10] = '0';
      data[11] = '1';
	}else{
      data[8] = 'V';
      data[9] = 'P';
      data[10] = '9';
      data[11] = '0';
	}

    /*Video width and height*/
    data[12] = width & 0xff;
    data[13] = (width >> 8) & 0xff;
    data[14] = height & 0xff;
    data[15] = (height >> 8) & 0xff;

    /*Frame rate*/
    data[16] = rateNum & 0xff;
    data[17] = (rateNum >> 8) & 0xff;
    data[18] = (rateNum >> 16) & 0xff;
    data[19] = (rateNum >> 24) & 0xff;

    /*Frame rate scale*/
    data[20] = rateDenom & 0xff;
    data[21] = (rateDenom >> 8) & 0xff;
    data[22] = (rateDenom >> 16) & 0xff;
    data[23] = (rateDenom >> 24) & 0xff;

    /*Video length in frames*/
    /*data[24] = (pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) & 0xff;
    data[25] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 8) & 0xff;
    data[26] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 16) & 0xff;
    data[27] = ((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic) >> 24) & 0xff;*/
    /*It's OK to not provide useful info*/
    data[24] = 0;
    data[25] = 0;
    data[26] = 0;
    data[27] = 0;

	memcpy(ivfHeader, data, 32);
}

static unsigned getPredataV2(unsigned char* prebuf, int index)
{
    assert(index < 14);
    int bit_index = index * 9;    //36

    int index_l = bit_index / 8;    //4
    int index_h = index_l + 1;     //5

    // skip_h + bits_h + bits_l + skip_l = 16 bits
    //          bits_h + bits_l          =  9 bits // valid data
    int skip_l  = bit_index % 8;  //4
    int bits_l = 8 - skip_l;    //4
    int bits_h = 9 - bits_l;     //5
    int skip_h = 8 - bits_h;     //3

    unsigned part_h = prebuf[index_h] & (0xFF >> skip_h);
    unsigned part_l = prebuf[index_l] >> skip_l;
    unsigned result = (part_h << bits_l) | part_l;

    return result;
}

static u8 *precarry9bitConvertOutput8bit(u8 *pre_carry_data, u8 *output_data, u32 av1pre_size, u32 output_size)
{
    u32 pre_data_length = av1pre_size ;
    u8 *precarry_data = pre_carry_data;

    u8 pre_data_offset = 16;
    u8 outout_offset = 14;
    precarry_data += pre_data_length;
    output_data += output_size;
    u8 pre_carry_tail_len = ((((output_size % 14) * 9 + 2) + 7) >> 3);
    u8 tail_flag = 0;
    u32 c = 0;
    u8 output_tail_len = output_size % 14;

    while(pre_data_length > 64){
        u32 offs = outout_offset;

        if(tail_flag == 0 && pre_carry_tail_len){
            precarry_data -= pre_carry_tail_len;
            pre_data_length -= pre_carry_tail_len;
            output_data -= output_tail_len;
            offs = output_tail_len;
            tail_flag++;
        }else{
            precarry_data -= pre_data_offset;
            pre_data_length -= pre_data_offset;
            output_data -= outout_offset;
        }

        while (offs > 0) {
            offs--;
            c = getPredataV2(precarry_data, offs) + c;
            output_data[offs] = (u8)c;
            c >>= 8;
        }
    }
    return output_data;
}

static u8 verifyOutputBitstream(u8 *pre_carry_data, u8 *output_data, u32 av1pre_size, u32 output_size)
{
    u8 pre_data_offset = 16;
    u8 outout_offset = 14;

    u32 pre_data_len = av1pre_size ;
    u8 pre_carry_tail_len = ((((output_size % 14) * 9 + 2) + 7) >> 3);
    u8 *precarry_data = pre_carry_data;

    precarry_data += pre_data_len;
    output_data += output_size;

    u8 tail_flag = 0;
    u32 c = 0;
    u8 output_tail_len = output_size % 14;
    while(pre_data_len > 0){
        u32  offs = outout_offset;
        if(tail_flag == 0 && pre_carry_tail_len){
            precarry_data -= pre_carry_tail_len;
            pre_data_len -= pre_carry_tail_len;
            output_data -= output_tail_len;
            offs = output_tail_len;
            tail_flag++;
        }else{
            precarry_data -= pre_data_offset;
            pre_data_len -= pre_data_offset;
            output_data -= outout_offset;
        }

        while (offs > 0) {
            offs--;
            c = getPredataV2(precarry_data, offs) + c;
            output_data[offs] = (u8)c;
            c >>= 8;
        }
    }

    return 0;
}

static void generateIvfFrameHeader(u8 *stream, u32 *ivfFrameCnt, u8 frameNotShow, u32 streamSize)
{
    i32 byteCnt = 0;
    u64 frameCntOut = *ivfFrameCnt;
    u8 data[12]; //Ivf frame header size

    byteCnt = streamSize;
    data[0] = byteCnt & 0xff;
    data[1] = (byteCnt >> 8) & 0xff;
    data[2] = (byteCnt >> 16) & 0xff;
    data[3] = (byteCnt >> 24) & 0xff;

    /*Time stamp*/
    data[4]  =  (frameCntOut)        & 0xff;
    data[5]  = ((frameCntOut) >> 8)  & 0xff;
    data[6]  = ((frameCntOut) >> 16) & 0xff;
    data[7]  = ((frameCntOut) >> 24) & 0xff;
    data[8]  = ((frameCntOut) >> 32) & 0xff;
    data[9]  = ((frameCntOut) >> 40) & 0xff;
    data[10] = ((frameCntOut) >> 48) & 0xff;
    data[11] = ((frameCntOut) >> 56) & 0xff;

    memcpy(stream, data, 12);

    /* IVF time stamp only increase when current frame in IVF is shown(show/show_existing) */
    if(!frameNotShow) *ivfFrameCnt += 1;
}

static void writeIvfFrameHeader(struct va_enc_channel *chn, VCEncOut *encOut,
                              EWLLinearMem_t *outputBuffer,  u32 *offset)
{
    struct video_encoder_private_context *ctx =
    (struct video_encoder_private_context *)chn->private_context;
    int streamSize = 0;
    u32 *pNaluSizes = encOut->pNaluSizeBuf;

    for(uint32_t i = 0; i < encOut->numNalus; i++) {
        streamSize += pNaluSizes[i];
    }
    for(uint32_t i = 0; i < encOut->numNalus; i++){
        memmove(((uint8_t *)outputBuffer->virtualAddress + *offset + 12),  ((uint8_t *)outputBuffer->virtualAddress + *offset), streamSize);
        uint8_t frameNotShow = (encOut->av1Vp9FrameNotShow >> i) & 1;
        generateIvfFrameHeader(((uint8_t *)outputBuffer->virtualAddress + *offset), &ctx->ivfFrameCnt, frameNotShow, pNaluSizes[i]);
        *offset += 12 + pNaluSizes[i];
        streamSize -= pNaluSizes[i];
    }
}

static void generateAV1stream(struct va_enc_channel *chn, VCEncOut *encOut,
                              EWLLinearMem_t *outputBuffer, u32 *offset)
{
    /* The stream buffer is mapped into the CPU address space, the hardware
       output only has to be made visible for the CPU. The IVF frame header is
       generated from the NAL unit sizes of the encoder. */
    (void)EWLSyncMemData(outputBuffer, 0, outputBuffer->size, DEVICE_TO_HOST);
    writeIvfFrameHeader(chn, encOut, outputBuffer, offset);
}

static vmppResult generateHeaders(struct va_enc_channel *chn,
                                  struct video_encoder_private_context *ctx, VCEncIn *encIn,
                                  VCEncOut *encOut, EWLLinearMem_t *outputBuffer)
{
    VCEncRet vRet;
    vRet = VCEncStrmStart(chn->codec_inst, encIn, encOut);
    if (vRet != VCENC_OK) {
        LOG_ERROR(ENC, "VCEncStrmStart failed: %d", vRet);
        set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
        return vmpp_RSLT_ERR_ENC_SEND_FRAME;
    }

    void *psData = (void *)outputBuffer->virtualAddress;
    uint32_t psDataSize = VA_MIN(encOut->streamSize, outputBuffer->size);
    uint32_t psDataBufferSize = 0;
    if (ctx->cfg.streamType != VCENC_BYTE_STREAM) {
        psDataBufferSize = psDataSize + encOut->numNalus * 4;
        if (!ctx->parametersSet) {
            ctx->parametersSet = (uint8_t *)malloc(psDataBufferSize);
            if (!ctx->parametersSet) {
                LOG_ERROR(ENC, "Fail to malloc memory for parameters set, size %d",
                          psDataBufferSize);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return vmpp_RSLT_ERR_NO_MEMORY;
            }
            //memset(ctx->parametersSet, 0, psDataBufferSize);
        }

        ctx->parametersSetSize = 0;

        uint32_t offset = 0;
        uint32_t dstOffset = 0;
        const uint8_t start_code_prefix[4] = {0x0, 0x0, 0x0, 0x1};
        uint32_t count = 0;
        uint32_t size;
        uint32_t numNalus = encOut->numNalus;
        const uint32_t *pNaluSizeBuf = encOut->pNaluSizeBuf;
        while (count++ < numNalus && *pNaluSizeBuf != 0) {
            memcpy(ctx->parametersSet + dstOffset, start_code_prefix, 4);
            ctx->parametersSetSize += 4;
            dstOffset += 4;
            size = *pNaluSizeBuf++;

            memcpy(ctx->parametersSet + dstOffset, psData + offset, size);
            ctx->parametersSetSize += size;
            offset += size;
            dstOffset += size;
        }
        ctx->parametersSetReady = 1;
    } else if (IS_AV1(ctx->codecFormat)){
        i32 width  = ctx->cfg.width;
        i32 height = ctx->cfg.height;
        if (!ctx->ivfHeader) {
            ctx->ivfHeader = (uint8_t *)malloc(32);
            if (!ctx->ivfHeader) {
                LOG_ERROR(ENC, "Fail to malloc memory for parameters set, size %d",
                            psDataBufferSize);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return vmpp_RSLT_ERR_NO_MEMORY;
            }
        }
        writeIvfHeader(ctx->ivfHeader, width, height, ctx->cfg.frameRateNum, ctx->cfg.frameRateDenom, 0);
        ctx->ivfHeaderSize = 32;
        ctx->ivfHeaderReady = 1;
    } else {
        psDataBufferSize = psDataSize;
        if (!ctx->parametersSet) {
            ctx->parametersSet = (uint8_t *)malloc(psDataBufferSize);
            if (!ctx->parametersSet) {
                LOG_ERROR(ENC, "Fail to malloc memory for parameters set, size %d",
                          psDataBufferSize);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return vmpp_RSLT_ERR_NO_MEMORY;
            }
            //memset(ctx->parametersSet, 0, psDataBufferSize);
        }

        memcpy(ctx->parametersSet, psData, psDataSize);
        ctx->parametersSetSize = psDataSize;
        ctx->parametersSetReady = 1;
    }

    return vmpp_RSLT_OK;
}

static vmppResult loadROIMap(struct video_encoder_private_context *ctx, const vmppFrame *frame,
                             uint8_t *deltaqp, int8_t *roimap)
{
    int32_t blkSize = 64 >> (ctx->roiMapDeltaQpBlockUnit & 3);
    uint32_t inWidth = (frame->width + blkSize - 1) / blkSize;
    if (IS_H264(ctx->codecFormat)) {
        for (uint32_t outY = 0; outY < frame->height; outY += 8) {
            for (uint32_t outX = 0; outX < frame->width; outX += 8) {
                uint32_t blk = (outY / 16) * ((frame->width + 15) / 16) + outX / 16;
                uint32_t idx = (outY % 16 / 8) * 2 + outX % 16 / 8;
                uint8_t roi = ((-roimap[outY / blkSize * inWidth + outX / blkSize]) << 1) & 0x7E;
                deltaqp[blk * 4 + idx] = roi;
            }
        }
    } else {
        for (uint32_t outY = 0; outY < frame->height; outY += 8) {
            for (uint32_t outX = 0; outX < frame->width; outX += 8) {
                uint32_t blk = (outY / 64) * ((frame->width + 63) / 64) + outX / 64;
                uint32_t idx = (outY % 64 / 8) * 8 + outX % 64 / 8;
                uint8_t roi = ((-roimap[outY / blkSize * inWidth + outX / blkSize]) << 1) & 0x7E;
                deltaqp[blk * 64 + idx] = roi;
            }
        }
    }
    return vmpp_RSLT_OK;
}

static vmppResult allocAndLoadROIMap(struct va_enc_channel *chn,
                                     struct video_encoder_private_context *ctx,
                                     const vmppFrame *frame, EWLLinearMem_t *deltaQp,
                                     EWLLinearMem_t *cuCtrlInfo, EWLLinearMem_t *cuCtrlIndex,
                                     vmppEncExtendedParams *extParams)
{
    int32_t max_cu_size = 64;
    if (IS_H264(ctx->codecFormat))
        max_cu_size = 16;
    uint32_t block_size = ((frame->width + max_cu_size - 1) & (~(max_cu_size - 1))) *
                          ((frame->height + max_cu_size - 1) & (~(max_cu_size - 1))) / (8 * 8 * 2);
    if (ctx->roiMapVersion >= 1)
        block_size *= 2;
    block_size = ((block_size + 63) & (~63));
    uint32_t buffer_size = block_size + ROIMAP_PREFETCH_EXT_SIZE;
    int allocCuCtrl = 0;

    const void *ewl_inst = ctx->ewlInst;

    if (deltaQp && deltaQp->busAddress && deltaQp->size < buffer_size) {
        EWLFreeLinear(ewl_inst, deltaQp);
        EWLFreeLinear(ewl_inst, cuCtrlInfo);
        EWLFreeLinear(ewl_inst, cuCtrlIndex);
        deltaQp->size = 0;
        cuCtrlInfo->size = 0;
        cuCtrlIndex->size = 0;
        allocCuCtrl = 1;

        deltaQp->mem_type = EWL_MEM_TYPE_VPU_WORKING;
        if (EWLMallocLinear(ewl_inst, buffer_size, 0, deltaQp) != EWL_OK) {
            LOG_ERROR(ENC, "Fail to re-malloc deltaQp buffer, size %d", buffer_size);
            return vmpp_RSLT_ERR_ENC_EWL;
        }
    } else if (deltaQp && !deltaQp->busAddress) {
        deltaQp->mem_type = EWL_MEM_TYPE_VPU_WORKING;
        if (EWLMallocLinear(ewl_inst, buffer_size, 0, deltaQp) != EWL_OK) {
            LOG_ERROR(ENC, "Fail to malloc deltaQp buffer, size %d", buffer_size);
            return vmpp_RSLT_ERR_ENC_EWL;
        }
        allocCuCtrl = 1;
    }
    assert(deltaQp->busAddress);

    if (allocCuCtrl) {
        uint8_t u8CuInfoSize = 1;
        if (chn->params.videoConfig.roiCuCtrlVersion == 3)
            u8CuInfoSize = 1;
        else if (chn->params.videoConfig.roiCuCtrlVersion == 4)
            u8CuInfoSize = 2;
        else if (chn->params.videoConfig.roiCuCtrlVersion == 5)
            u8CuInfoSize = 6;
        else if (chn->params.videoConfig.roiCuCtrlVersion == 6)
            u8CuInfoSize = 12;
        else // if((chn->params.videoConfig.roiCuCtrlVersion == 7)
            u8CuInfoSize = 14;
        cuCtrlInfo->mem_type = EWL_MEM_TYPE_VPU_WORKING;
        if (EWLMallocLinear(ewl_inst, (block_size * u8CuInfoSize), 0, cuCtrlInfo) != EWL_OK) {
            cuCtrlInfo->virtualAddress = NULL;
            EWLFreeLinear(ewl_inst, deltaQp);
            LOG_ERROR(ENC,
                      "Fail to malloc cuCtrlInfo buffer, size %d, block_size %d, u8CuInfoSize %d",
                      (block_size * u8CuInfoSize), block_size, u8CuInfoSize);
            return vmpp_RSLT_ERR_ENC_EWL;
        }
        dmaMemZero(cuCtrlInfo, cuCtrlInfo->size);

        block_size = 1 << (IS_H264(ctx->codecFormat) ? 4 : 6);
        block_size = ((frame->width + block_size - 1) & (~(block_size - 1))) *
                     ((frame->height + block_size - 1) & (~(block_size - 1))) /
                     (block_size * block_size);

        cuCtrlIndex->mem_type = EWL_MEM_TYPE_VPU_WORKING;
        if (EWLMallocLinear(ewl_inst, block_size, 0, cuCtrlIndex) != EWL_OK) {
            cuCtrlIndex->virtualAddress = NULL;
            EWLFreeLinear(ewl_inst, deltaQp);
            EWLFreeLinear(ewl_inst, cuCtrlInfo);
            LOG_ERROR(ENC, "Fail to malloc cuCtrlIndex buffer, size %d", block_size);
            return vmpp_RSLT_ERR_ENC_EWL;
        }
        dmaMemZero(cuCtrlIndex, cuCtrlIndex->size);
    }

    if (extParams && extParams->roiMap.roiMapDeltaQp && extParams->roiMap.roiMapDeltaQpSize) {
        if (deltaQp->virtualAddress == NULL)
            return vmpp_RSLT_ERR_NO_MEMORY;

        memset(deltaQp->virtualAddress, 0, deltaQp->size);
        loadROIMap(ctx, frame, (uint8_t *)deltaQp->virtualAddress, extParams->roiMap.roiMapDeltaQp);
        (void)EWLSyncMemData(deltaQp, 0, deltaQp->size, HOST_TO_DEVICE);
    } else {
        dmaMemZero(deltaQp, deltaQp->size);
    }

    return vmpp_RSLT_OK;
}

static vmppResult allocROIMapRes(struct va_enc_channel *chn, const vmppFrame *frame,
                                 vmppEncExtendedParams *extParams, EncInputBuffer *inputBuffer,
                                 uint32_t timeout)
{
    UNUSED_PARAMETER(timeout);
    vmppResult ret = vmpp_RSLT_OK;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    EWLLinearMem_t *deltaQp = NULL;
    EWLLinearMem_t *cuCtrlInfo = NULL;
    EWLLinearMem_t *cuCtrlIndex = NULL;

    int idx = getIdleROIMem(ctx, &deltaQp, &cuCtrlInfo, &cuCtrlIndex);
    if (idx < 0) {
        LOG_WARN(ENC, "No available ROI map memory");
        return vmpp_RSLT_ERR_NO_BUFFER;
    }

    if (extParams && extParams->roiMap.roiMapDeltaQp && extParams->roiMap.roiMapDeltaQpSize &&
        ctx->roiType == vmpp_ENC_ROI_MAP) {
        ret = allocAndLoadROIMap(chn, ctx, frame, deltaQp, cuCtrlInfo, cuCtrlIndex, extParams);
        if (ret != vmpp_RSLT_OK) {
            setROIMemIdle(ctx, deltaQp);
            return ret;
        }

        inputBuffer->roiMapDeltaQpSize = extParams->roiMap.roiMapDeltaQpSize;
    } else {
        ret = allocAndLoadROIMap(chn, ctx, frame, deltaQp, cuCtrlInfo, cuCtrlIndex, NULL);
        if (ret != vmpp_RSLT_OK) {
            setROIMemIdle(ctx, deltaQp);
            return ret;
        }
        inputBuffer->roiMapDeltaQpSize = 0;
    }
    inputBuffer->roiMapDeltaQpMem = deltaQp;
    inputBuffer->roimapCuCtrlInfoMem = cuCtrlInfo;
    inputBuffer->roimapCuCtrlIndexMem = cuCtrlIndex;

    return vmpp_RSLT_OK;
}

vmppResult handle_dynamic_resolution(struct va_enc_channel *chn, EncInputBuffer *inputBuffer,
                                     EWLLinearMem_t *outputBuffer, int *newEncoder, int need_flush)
{
    vmppResult ret = vmpp_RSLT_OK;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;

    if (need_flush > 0) {
        ctx->internalFlushing = 1;
        ctx->firstFrameNumberOfNewRes = inputBuffer->number;
        ctx->nextGopSize = 1;
        LOG_DEBUG(ENC, "Do internal flush, inputBuffer->number %d, need flush number %d",
                    inputBuffer->number, need_flush);
    } else {
        LOG_DEBUG(
            ENC,
            "Start encoding first frame of new resolution: REMAIN BUFFER %d,"
            "inputBuffer->number %d, need flush number %d",
            getNotEncodedBufferCnt(ctx), inputBuffer->number, need_flush);
        ctx->internalFlushing = 0;
        vmppFrame dummyFrame = {0};
        dummyFrame.width = inputBuffer->width;
        dummyFrame.height = inputBuffer->height;
        dummyFrame.pixelFormat = inputBuffer->format;
        dummyFrame.stride[0] = inputBuffer->stride[0];
        ctx->numberBase = inputBuffer->number;
        uint64_t start = va_gettime_ns();
        ret = recreateEncoder(chn, &chn->params, &dummyFrame);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR(ENC, "Recreating encoder failed: new res %dx%d fmt %d",
                        inputBuffer->width, inputBuffer->height, inputBuffer->format);
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            return ret;
        }
        uint64_t end = va_gettime_ns();
        LOG_DEBUG(ENC, "Start encoding first frame of new resolution ---2 --- time %lld",
                    (U64) ((end - start) / 1000));
        // SetupOutputBuffer
        encIn->busOutBuf[0] = outputBuffer->busAddress;
        encIn->outBufSize[0] = outputBuffer->size;
    #if defined (ANDROID_32BIT) || defined(X86_32)
        encIn->pOutBuf[0].LSB = outputBuffer->virtualAddress;
    #else
        encIn->pOutBuf[0] = outputBuffer->virtualAddress;
    #endif
        encIn->poc = 0;
        encIn->gopPicIdx = 0;
        ctx->parametersSetOutputed = 0;
        ctx->ivfHeaderOutputed = 0;
        ret = generateHeaders(chn, ctx, encIn, encOut, outputBuffer);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR(ENC, "Generating header failed: %d", ret);
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            return ret;
        }
        // update width & height for dynamic resolution
        ctx->lastEncDummyFrame.width = inputBuffer->width;
        ctx->lastEncDummyFrame.height = inputBuffer->height;
        *newEncoder = 1;
    }
    return ret;
}

vmppResult video_find_next_pic(struct va_enc_channel *chn)
{
    vmppResult ret = vmpp_RSLT_OK;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    uint32_t adaptiveGop = (chn->params.videoConfig.gopSize == 0);

    if (ctx->encInLast.insertIDR && ctx->encInLast.codingType == VCENC_INTRA_FRAME
        && (ctx->lastVRet == VCENC_FRAME_READY || ctx->lastVRet == VCENC_FRAME_ENQUEUE)) {
        ctx->nextGopSize = (adaptiveGop ? 4 : chn->params.videoConfig.gopSize);
        encIn->gopSize = ctx->nextGopSize;
        ctx->nextCodingType = findNextPictureType(encIn, ctx->nextGopSize);
        ctx->lastIPFramePoc = ctx->encInLast.picture_cnt + ctx->numberBase;
        ctx->curIPFramePoc = encIn->picture_cnt + ctx->numberBase;
    } else {
        if (ctx->lastVRet == VCENC_FRAME_ENQUEUE) {
            if (adaptiveGop && chn->params.videoConfig.lookaheadDepth)
            {
                if (ctx->parallelCoreNum > 1)
                {
                    getNextGopSize(&ctx->lastEncDummyFrame, encIn, chn->codec_inst, ctx->cfg.gopMaxBSize,
                                &ctx->nextGopSize, &ctx->agop,
                                chn->params.videoConfig.lookaheadDepth, encOut);
                }
                else
                {
                    getNextGopSizeSingleThread(&ctx->lastEncDummyFrame, encIn, chn->codec_inst, ctx->cfg.gopMaxBSize,
                                &ctx->nextGopSize, &ctx->agop,
                                chn->params.videoConfig.lookaheadDepth, encOut);
                }
            }
            else if (chn->params.videoConfig.lookaheadDepth)
            {
                /* The GOP size of pass 1 is not provided by this library version. */
            }
            ctx->nextCodingType = findNextPictureType(encIn, ctx->nextGopSize);
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
        } else if (ctx->lastVRet == VCENC_FRAME_READY) {
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            if (adaptiveGop)
            {
                if (ctx->parallelCoreNum > 1)
                {
                    getNextGopSize(&ctx->lastEncDummyFrame, encIn, chn->codec_inst, ctx->cfg.gopMaxBSize,
                                &ctx->nextGopSize, &ctx->agop,
                                chn->params.videoConfig.lookaheadDepth, encOut);
                }
                else
                {
                    getNextGopSizeSingleThread(&ctx->lastEncDummyFrame, encIn, chn->codec_inst, ctx->cfg.gopMaxBSize,
                                &ctx->nextGopSize, &ctx->agop,
                                chn->params.videoConfig.lookaheadDepth, encOut);
                }

            }
            else if (chn->params.videoConfig.lookaheadDepth)
            {
                /* The GOP size of pass 1 is not provided by this library version. */
            }
            ctx->nextCodingType = findNextPictureType(encIn, ctx->nextGopSize);
        }

        int tmpPoc = MAX(ctx->curIPFramePoc, encIn->picture_cnt + ctx->numberBase);
        if (tmpPoc != ctx->curIPFramePoc) {
            ctx->lastIPFramePoc = ctx->curIPFramePoc;
            ctx->curIPFramePoc = tmpPoc;
        }
    }

    if (ctx->lastIPFramePoc < ctx->insertIdrPicCnt) {
        // set gopChangeIdr = 1 for frames before insertIDR frame
        ctx->lastIPFramePoc = setInputBufferGopChangeIdr(ctx);
    }

    return ret;
}

void video_insert_idr_gopchange(struct va_enc_channel *chn, EncInputBuffer **inputBuffer, int *inputIndex)
{
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;

    int gopChangeIdrIndex = getInputBufferGopChangeIdrIndex(ctx);
    if (gopChangeIdrIndex >= 0 &&
        (*inputBuffer)->number >= ctx->pictureMem[gopChangeIdrIndex]
                                .number) // encode remaining frames and idr frames
    {
        int gopChangeCnt = 0;
        changeInputBufferGopChangeIdrIndex(ctx, &gopChangeCnt);
        *inputBuffer = &ctx->pictureMem[gopChangeIdrIndex];
        *inputIndex = gopChangeIdrIndex;
        if ((*inputBuffer)->forceIDR) {
            encIn->picture_cnt = (*inputBuffer)->number - ctx->numberBase;
        } else if (gopChangeCnt > 0) {
            encIn->codingType = ctx->encInLast.codingType;
            encIn->poc = ctx->encInLast.poc;
            encIn->gopSize = ctx->encInLast.gopSize;
            encIn->gopPicIdx = ctx->encInLast.gopPicIdx;
            encIn->picture_cnt = ctx->encInLast.picture_cnt;
            if (gopChangeCnt > 4)
                ctx->nextGopSize = 4;
            else
                ctx->nextGopSize = gopChangeCnt;
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            ctx->nextCodingType =
                findNextPictureType(encIn, ctx->nextGopSize);
            int32_t expNum = ctx->internalFlushing ? ctx->firstFrameNumberOfNewRes
                                : encIn->picture_cnt + ctx->numberBase;
            *inputIndex = getInputBuffer(ctx, expNum, inputBuffer);
        }
    }
}

static vmppResult checkParamsAndStatus(struct video_encoder_private_context *ctx, vmppFrame *frame)
{
    if (ctx->hashLenMismatch) {
        return vmpp_RSLT_ERR_ENC_DRIVER_MISMATCH;
    }

    if (ctx->eos) {
        return vmpp_RSLT_WARN_EOS;
    }

    if (!IS_AV1(ctx->codecFormat) && !ctx->parametersSetReady && frame->memoryType == vmpp_MEM_FLUSH) {
        LOG_ERROR(ENC, "First frame should not be a flush frame!!!");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    if (IS_AV1(ctx->codecFormat) && !ctx->ivfHeaderReady && frame->memoryType == vmpp_MEM_FLUSH) {
        LOG_ERROR(ENC, "First frame should not be a flush frame!!!");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    return vmpp_RSLT_OK;
}

/* Copies a host frame that is not stored with the aligned stride of the
   encoder into the input buffer, the planes are copied back to back. */
static void HybridDMATransWrite(vmppFrame *frame, EncInputBuffer *inputBuffer, uint64_t lumaSize,
                                uint64_t chromaSize)
{
    uint8_t *dst = (uint8_t *)inputBuffer->mem.virtualAddress;
    int planes;

    if (dst == NULL || frame->data[0] == NULL)
        return;

    if (frame->pixelFormat == vmpp_PIX_FMT_YUV420P ||
        frame->pixelFormat == vmpp_PIX_FMT_YUV420_PLANAR_10BIT_LE) {
        planes = 3;
    } else if (frame->pixelFormat == vmpp_PIX_FMT_RGBA) {
        planes = 1;
    } else {
        planes = 2;
    }

    /* The planes of the frame are continuous, they are copied in one block. */
    if (planes < 2 || (frame->data[0] + lumaSize == frame->data[1])) {
        bool continuous = true;
        for (int i = 1; i < planes; i++) {
            if (frame->data[i] == NULL)
                break;
            if (i == 1 && frame->data[0] + lumaSize != frame->data[1])
                continuous = false;
            if (i == 2 && frame->data[2] != NULL &&
                frame->data[1] + chromaSize / 2 != frame->data[2])
                continuous = false;
        }
        if (continuous && frame->dataSize > 0) {
            memcpy(dst, frame->data[0], frame->dataSize);
            (void)EWLSyncMemData(&inputBuffer->mem, 0, (u32)(lumaSize + chromaSize),
                                 HOST_TO_DEVICE);
            return;
        }
    }

    uint64_t offset = 0;
    if (planes < 2 || (frame->data[0] + lumaSize == frame->data[1])) {
        if (frame->data[0] != NULL) {
            memcpy(dst + offset, frame->data[0], lumaSize + chromaSize / 2);
            offset += lumaSize + chromaSize / 2;
        }
        if (frame->data[2] != NULL)
            memcpy(dst + offset, frame->data[2], chromaSize / 2);
    } else {
        if (frame->data[0] != NULL) {
            memcpy(dst + offset, frame->data[0], lumaSize);
            offset += lumaSize;
        }
        if (frame->data[1] != NULL) {
            memcpy(dst + offset, frame->data[1], chromaSize);
            offset += chromaSize;
        }
        if (planes == 3 && frame->data[2] != NULL &&
            frame->data[1] + chromaSize / 2 != frame->data[2])
            memcpy(dst + offset, frame->data[2], chromaSize / 2);
    }

    (void)EWLSyncMemData(&inputBuffer->mem, 0, (u32)(lumaSize + chromaSize), HOST_TO_DEVICE);
}

static vmppResult preProcInputAndOutput(struct va_enc_channel *chn, vmppFrame *frame, vmppEncExtendedParams *extParams,
    EncInputBuffer **ppInputBuffer, EWLLinearMem_t **ppOutputBuffer, int *pFrameSaved, uint32_t timeout)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    int inputIndex = 0;
    vmppResult ret;
    u32 size;
    u32 scale;
    u8 *av1Header = NULL;
    EWLLinearMem_t *outputBuffer = NULL;
    EncInputBuffer *inputBuffer = NULL;
    outputBuffer = getIdleOutputBuffer(ctx);
    if (!outputBuffer) {
        LOG_ERROR(ENC, "No available output buffer.");
        return vmpp_RSLT_ERR_NO_BUFFER;
    }

    inputIndex = getIdleInputBuffer(ctx, &inputBuffer);
    if (inputIndex < 0) {
        LOG_ERROR(ENC, "No available input buffer.");
        return vmpp_RSLT_ERR_NO_BUFFER;
    }

    /* If current frame memtype is FLUSH, it need set FLUSH to the last frame.
       Otherwise won't do flush. */
    if (frame->memoryType == vmpp_MEM_FLUSH) {
        ctx->lastInputFrame.memoryType = vmpp_MEM_FLUSH;
    }

    ret = allocRes(chn, frame->memoryType == vmpp_MEM_FLUSH ? &ctx->lastInputFrame : frame, outputBuffer,
        &inputBuffer->mem, inputBuffer, timeout);
    if (ret != vmpp_RSLT_OK) {
        setInputBufferIdle(ctx, inputBuffer);
        set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
        return ret;
    }

    if (IS_AV1(ctx->codecFormat) && ctx->workmode == MULTI_CORE_MODE) {
        ret = getIdleAv1HeaderOutputBuffer(ctx, &av1Header);
    }

    if (frame->memoryType == vmpp_MEM_FLUSH || ctx->internalFlushing) {
        LOG_DEBUG(ENC, "frame->memoryType %d, ctx->internalFlushing %d", vmpp_MEM_FLUSH, ctx->internalFlushing);
        setInputBufferIdle(ctx, inputBuffer);
        inputBuffer = NULL;
    } else {
        inputBuffer->width = frame->width;
        inputBuffer->height = frame->height;
        inputBuffer->format = frame->pixelFormat;
        inputBuffer->stride[0] = frame->stride[0];
        inputBuffer->newResolution = 0;
        if (!(ctx->lastEncDummyFrame.width == 0 || ctx->lastEncDummyFrame.height == 0) &&
            (inputBuffer->width != ctx->lastEncDummyFrame.width ||
                inputBuffer->height != ctx->lastEncDummyFrame.height) &&
            (inputBuffer->width != ctx->lastInputFrame.width || inputBuffer->height != ctx->lastInputFrame.height)) {
            LOG_INFO(ENC, "New Res: %dx%d, Orig Res: %dx%d, number %d", inputBuffer->width, inputBuffer->height,
                ctx->lastEncDummyFrame.width, ctx->lastEncDummyFrame.height, ctx->inputPictureCount);
#ifndef ENABLE_DYNAMIC_RES
            setInputBufferIdle(ctx, inputBuffer);
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            LOG_ERROR(ENC, "Dynamic resolution encoder is not supported, New Res: %dx%d, Orig Res: %dx%d, number %d",
                inputBuffer->width, inputBuffer->height, ctx->lastEncDummyFrame.width, ctx->lastEncDummyFrame.height,
                ctx->inputPictureCount);
            return vmpp_RSLT_ERR_UNSUPPORTED;
#endif
            inputBuffer->newResolution = 1;
        }
        inputBuffer->memType = frame->memoryType;
        inputBuffer->number = ctx->inputPictureCount++;
        inputBuffer->pts = frame->pts;
        inputBuffer->timebaseDen = frame->timebase.denominator;
        inputBuffer->timebaseNum = frame->timebase.numerator;
        inputBuffer->sent2Encoder = 0;
        inputBuffer->forceIDR = extParams ? !!extParams->forceIDR : 0;
        // first frame of new resolution must be IDR
        if (inputBuffer->newResolution) {
            inputBuffer->forceIDR = 1;
        }
        inputBuffer->gopChangeIdr = 0;    // for insertIDR
        inputBuffer->roiType = vmpp_ENC_ROI_NONE;
        inputBuffer->updateTypeMask = extParams && extParams->updateTypeMask ? extParams->updateTypeMask : 0;
        inputBuffer->updateBitRate = inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_CBR ? extParams->updateBitRate : 0;
        inputBuffer->updateVbvBufSize =
            inputBuffer->updateTypeMask & (VMPP_ENC_UPDATE_CBR | VMPP_ENC_UPDATE_CRF) ? extParams->updateVbvBufSize : 0;
        inputBuffer->updateVbvMaxRate =
            inputBuffer->updateTypeMask & (VMPP_ENC_UPDATE_CBR | VMPP_ENC_UPDATE_CRF) ? extParams->updateVbvMaxRate : 0;
        inputBuffer->updateFrameRate = inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_FRAMERATE ?
                                           extParams->updateFrameRate :
                                           chn->params.videoConfig.frameRate;
        inputBuffer->updateCrf = inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_CRF ? extParams->updateCrf : -1;
        inputBuffer->updateKeyInt = inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_KEYINT ? extParams->updateKeyInt : 0;
        if (inputBuffer->updateKeyInt && !inputBuffer->forceIDR) {
            inputBuffer->forceIDR = 2;    // 2 for updateKeyInt, 1 for insertIDR
        }
        if (inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_QP) {
            inputBuffer->updateInitQp = extParams->updateQpSetting.updateInitQp;
            inputBuffer->updateQpMinI = extParams->updateQpSetting.updateQpMinI;
            inputBuffer->updateQpMaxI = extParams->updateQpSetting.updateQpMaxI;
            inputBuffer->updateQpMinPB = extParams->updateQpSetting.updateQpMinPB;
            inputBuffer->updateQpMaxPB = extParams->updateQpSetting.updateQpMaxPB;
        }

        if (extParams && ctx->roiType == vmpp_ENC_ROI_RANGE) {
            memcpy(&inputBuffer->roi, &extParams->roi, sizeof(extParams->roi));
            inputBuffer->roiType = vmpp_ENC_ROI_RANGE;
        } else if (extParams && ctx->roiType == vmpp_ENC_ROI_MAP) {
            ret = allocROIMapRes(chn, frame, extParams, inputBuffer, timeout);
            if (ret != vmpp_RSLT_OK) {
                setInputBufferIdle(ctx, inputBuffer);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return ret;
            }
            inputBuffer->roiType = vmpp_ENC_ROI_MAP;
        }

        // alloc roi map for hevc 2 pass workaround
        if (ctx->roiType != vmpp_ENC_ROI_MAP && chn->params.videoConfig.lookaheadDepth &&
            (IS_HEVC(ctx->codecFormat) || IS_AV1(ctx->codecFormat))) {
            ret = allocROIMapRes(chn, frame, NULL, inputBuffer, timeout);
            if (ret != vmpp_RSLT_OK) {
                setInputBufferIdle(ctx, inputBuffer);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return ret;
            }
        }

        if (!IS_AV1(ctx->codecFormat) && frame && frame->seiCount) {
            ret = saveSEI(ctx, inputBuffer, frame);
            if (ret < 0) {
                if (inputBuffer->roiMapDeltaQpMem) {
                    setROIMemIdle(ctx, inputBuffer->roiMapDeltaQpMem);
                }
                setInputBufferIdle(ctx, inputBuffer);
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                return ret;
            }
        }
        if (frame->memoryType == vmpp_MEM_HOST) {
#if 0
            FILE* fp = fopen("input.yuv", "wb");
            fwrite(frame->data[0], 1, frame->dataSize, fp);
            fclose(fp);
#endif
            if (chn->params.videoConfig.alignmentEnable == 0) {
                HybridDMATransWrite(frame, inputBuffer, inputBuffer->lumaSize,
                                    inputBuffer->chromaSize);
            } else {
                ret = copyInputPicture(frame, &inputBuffer->mem, inputBuffer->lumaSize,
                                       inputBuffer->chromaSize);
                if (ret != vmpp_RSLT_OK) {
                    LOG_ERROR(ENC, "Failed to copy the input picture: %d", ret);
                    setInputBufferIdle(ctx, inputBuffer);
                    set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                    return ret;
                }
            }
        }
        // save information of the last valid frame
        ctx->lastInputFrame = *frame;
        if (ctx->lastEncDummyFrame.width == 0 || ctx->lastEncDummyFrame.height == 0) {
            ctx->lastEncDummyFrame = *frame;
        }

        *pFrameSaved = 1;

        if (inputBuffer && inputBuffer->newResolution) {
            ctx->newResPicCnt = inputBuffer->number;
        }

        if (inputBuffer && inputBuffer->forceIDR) {
            inputBuffer->gopChangeIdr = 1;
            ctx->insertIdrPicCnt = inputBuffer->number;
        }
    }

    if (IS_AV1(ctx->codecFormat)) {
        scale = 3;
        size = (u32)(outputBuffer->size / scale);    // AV1 precarry buf is following to outputbuf
    } else {
        size = outputBuffer->size;
    }

    // SetupOutputBuffer
    encIn->busOutBuf[0] = outputBuffer->busAddress;
    encIn->outBufSize[0] = size;
#if defined(ANDROID_32BIT) || defined(X86_32)
    if (IS_AV1(ctx->codecFormat) && ctx->workmode == MULTI_CORE_MODE) {
        encIn->pOutBuf[0].LSB = (u32 *)av1Header;
    } else {
        encIn->pOutBuf[0].LSB = outputBuffer->virtualAddress;
    }
#else
    if (IS_AV1(ctx->codecFormat) && ctx->workmode == MULTI_CORE_MODE) {
        encIn->pOutBuf[0] = (u32 *)av1Header;
    } else {
        encIn->pOutBuf[0] = outputBuffer->virtualAddress;
    }
#endif

    /* The AV1 precarry output buffer is not provided by this library version. */
    UNUSED_PARAMETER(scale);

    if ((!IS_AV1(ctx->codecFormat) && !ctx->parametersSetReady) || (IS_AV1(ctx->codecFormat) && !ctx->ivfHeaderReady)) {
        ret = generateHeaders(chn, ctx, encIn, encOut, outputBuffer);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR(ENC, "Generating header failed: %d", ret);
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            return ret;
        }
        ctx->nextCodingType = VCENC_INTRA_FRAME;
    }
    *ppInputBuffer = inputBuffer;
    *ppOutputBuffer = outputBuffer;
    return vmpp_RSLT_OK;
}

static vmppResult updateGopConfig(struct va_enc_channel *chn, vmppFrame *frame, vmppEncExtendedParams *extParams)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    vmppResult ret = vmpp_RSLT_OK;
    if (frame->memoryType == vmpp_MEM_FLUSH) {
        LOG_DEBUG(ENC, "No more input, set last picture number %d", ctx->inputPictureCount - 1);
        encIn->gopConfig.lastPic = ctx->inputPictureCount - 1;
    }

    encIn->gopConfig.pGopPicSpecialCfg[0].i32Interval = chn->params.videoConfig.ltrInterval;
    if (extParams) {
        if (extParams->forceLTR) {
            encIn->gopConfig.pGopPicSpecialCfg[0].i32Interval = encIn->poc + 1;
        }
    }

    if (!ctx->flushing && !ctx->internalFlushing) {
        ret = video_find_next_pic(chn);

        LOG_DEBUG(ENC, "nextGopSize %d, nextCodingType %d", ctx->nextGopSize, ctx->nextCodingType);
        LOG_DEBUG(ENC, "next picture_cnt %d", encIn->picture_cnt);

        // Reset last vRet
        ctx->lastVRet = VCENC_OK;
    }
    return ret;
}

static vmppResult handleFlush(struct va_enc_channel *chn, vmppStream *stream, EWLLinearMem_t *outputBuffer,
    EncInputBuffer **ppInputBuffer, int *pInputIndex, int *remainJobs)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    int32_t expNum;
    VCEncRet vRet;
    int inputIndex;
    EncInputBuffer *inputBuffer = NULL;
    LOG_DEBUG(ENC,
        "DEBUG: -vmpp_MEM_FLUSH- ctx->outputPictureCount %d, "
        "ctx->inputPictureCount %d, getNotEncodedBufferCnt(ctx) %d",
        ctx->outputPictureCount, ctx->inputPictureCount, getNotEncodedBufferCnt(ctx));
    // reach the end
    uint32_t offset = 0;
    if (!IS_AV1(ctx->codecFormat) && !ctx->parametersSetOutputed) {
        memcpy(outputBuffer->virtualAddress, ctx->parametersSet, ctx->parametersSetSize);
        offset = ctx->parametersSetSize;
        encIn->outBufSize[0] = outputBuffer->size - offset;
#if defined(ANDROID_32BIT) || defined(X86_32)
        encIn->pOutBuf[0].LSB = outputBuffer->virtualAddress + offset;
#else
        encIn->pOutBuf[0] = outputBuffer->virtualAddress + offset;
#endif
        ctx->parametersSetOutputed = 1;
    }
    if (IS_AV1(ctx->codecFormat) && !ctx->ivfHeaderOutputed) {
        memcpy(outputBuffer->virtualAddress, ctx->ivfHeader, ctx->ivfHeaderSize);
        offset = ctx->ivfHeaderSize;
        encIn->outBufSize[0] = outputBuffer->size - offset;
#if defined(ANDROID_32BIT) || defined(X86_32)
        encIn->pOutBuf[0].LSB = outputBuffer->virtualAddress + offset;
#else
        encIn->pOutBuf[0] = outputBuffer->virtualAddress + offset;
#endif
        ctx->ivfHeaderOutputed = 1;
    }
    if (chn->params.videoConfig.lookaheadDepth > 0 && ctx->outputPictureCount < ctx->inputPictureCount) {
        LOG_DEBUG(ENC,
            "DEBUG: -FLUSHING- ctx->outputPictureCount %d, "
            "ctx->inputPictureCount %d, getNotEncodedBufferCnt(ctx) %d",
            ctx->outputPictureCount, ctx->inputPictureCount, getNotEncodedBufferCnt(ctx));
        if (getNotEncodedBufferCnt(ctx)) {
            encIn->codingType = ctx->encInLast.codingType;
            encIn->poc = ctx->encInLast.poc;
            encIn->gopSize = ctx->encInLast.gopSize;
            encIn->gopPicIdx = ctx->encInLast.gopPicIdx;
            encIn->picture_cnt = ctx->encInLast.picture_cnt;
            ctx->nextGopSize =
                ctx->encInLast.gopSize ?
                    ctx->encInLast.gopSize :
                    ((int32_t)chn->params.videoConfig.gopSize > 0 ? (int32_t)chn->params.videoConfig.gopSize : 4);
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            ctx->nextCodingType =
                findNextPictureType(encIn, ctx->nextGopSize);
            expNum = encIn->picture_cnt + ctx->numberBase;
            inputIndex = getInputBuffer(ctx, expNum, ppInputBuffer);
            *pInputIndex = inputIndex;
        } else {
            ctx->flushing = 1;
        }
        *remainJobs = 1;
    } else {
        LOG_DEBUG(ENC,
            "DEBUG: -END- ctx->outputPictureCount %d, "
            "ctx->inputPictureCount %d, getNotEncodedBufferCnt(ctx) %d",
            ctx->outputPictureCount, ctx->inputPictureCount, getNotEncodedBufferCnt(ctx));
        if (getNotEncodedBufferCnt(ctx)) {
            encIn->codingType = ctx->encInLast.codingType;
            encIn->poc = ctx->encInLast.poc;
            encIn->gopSize = ctx->encInLast.gopSize;
            encIn->gopPicIdx = ctx->encInLast.gopPicIdx;
            encIn->picture_cnt = ctx->encInLast.picture_cnt;
            ctx->nextGopSize =
                ctx->encInLast.gopSize ?
                    ctx->encInLast.gopSize :
                    ((int32_t)chn->params.videoConfig.gopSize > 0 ? (int32_t)chn->params.videoConfig.gopSize : 4);
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            ctx->nextCodingType =
                findNextPictureType(encIn, ctx->nextGopSize);
            expNum = encIn->picture_cnt + ctx->numberBase;
            inputIndex = getInputBuffer(ctx, expNum, ppInputBuffer);
            *pInputIndex = inputIndex;
            *remainJobs = 1;
        } else {
            /* return bus address for user to release frame */
            while (getRemainInputBuffer(ctx, ppInputBuffer) >= 0) {
                if (ctx->workmode == MULTI_CORE_MODE) {
                    return video_multicore_flush(chn, ppInputBuffer, stream);
                }
                inputBuffer = *ppInputBuffer;
                if (inputBuffer->memType == vmpp_MEM_DEVICE) {
                    stream->stream = NULL;
                    stream->len = 0;
                    stream->pts = inputBuffer->pts;
                    stream->inputBusAddress = inputBuffer->mem.busAddress;
                    if (inputBuffer->roiMapDeltaQpMem) {
                        setROIMemIdle(ctx, inputBuffer->roiMapDeltaQpMem);
                    }
                    setInputBufferIdle(ctx, inputBuffer);
                    set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                    LOG_DEBUG(ENC,
                        "DEBUG: -FLUSHING- ctx->outputPictureCount %d, "
                        "ctx->inputPictureCount %d === FOR RELEASE",
                        ctx->outputPictureCount, ctx->inputPictureCount);
                    return vmpp_RSLT_ENC_FLUSH;
                }
                LOG_INFO(ENC, "Flush remained buffer: number %d", inputBuffer->number);
                if (inputBuffer->roiMapDeltaQpMem) {
                    setROIMemIdle(ctx, inputBuffer->roiMapDeltaQpMem);
                }
                setInputBufferIdle(ctx, inputBuffer);
            }
            LOG_INFO(ENC, "FLUSHING: input %d --> output %d", ctx->inputPictureCount, ctx->outputPictureCount);
#if defined(ANDROID_32BIT) || defined(X86_32)
            if (IS_AV1(ctx->codecFormat)) {
                encIn->pOutBuf[0].LSB = outputBuffer->virtualAddress;
            }
#else
            if (IS_AV1(ctx->codecFormat)) {
                encIn->pOutBuf[0] = outputBuffer->virtualAddress;
            }
#endif
            vRet = VCEncStrmEnd(chn->codec_inst, encIn, encOut);
            if (vRet == VCENC_OK) {
                if (IS_AV1(ctx->codecFormat)) {
                    writeIvfFrameHeader(chn, encOut, outputBuffer, &offset);
                    offset -= encOut->streamSize;
                }
                if (ctx->cfg.streamType == VCENC_BYTE_STREAM) {
                    stream->stream = (uint8_t *)outputBuffer->virtualAddress;
                    stream->len = encOut->streamSize + offset;
                    stream->pts = -1;
                    stream->inputBusAddress = (vmppDevAddr)NULL;
                }
                LOG_INFO(ENC, "Encoding Finished!!!");
                ctx->eos = 1;
                if (IS_AV1(ctx->codecFormat) && stream->len == 0) {
                    return vmpp_RSLT_WARN_EOS;
                }
                return vmpp_RSLT_OK;
            }
            assert(0 && "No more remained frame.");
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            return vmpp_RSLT_WARN_EOS;
        }
    }
    return vmpp_RSLT_OK;
}

static vmppResult finalizeVCEncIn(struct va_enc_channel *chn, EncInputBuffer *inputBuffer, vmppFrame *frame)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    vmppResult ret;
    uint32_t keyInt = chn->params.videoConfig.keyInt;
    uint8_t *aligned_base = NULL;
    size_t aligned_offset = 0;
    const int pgsize = getpagesize();
    const size_t page_mask = ~(pgsize - 1);
    // SetupInputBuffer
    encIn->busLuma = inputBuffer->mem.busAddress;
    const bool is_y_discontinuous = frame->data[0] && frame->data[1] && (frame->data[0] + inputBuffer->lumaSize != frame->data[1]);
    if (is_y_discontinuous) {
        encIn->busChromaU = ALIGN_4K(encIn->busLuma + inputBuffer->lumaSize);
        CALC_ALIGNED_ADDR(frame->data[1], aligned_base, aligned_offset, page_mask);
        encIn->busChromaU += aligned_offset;
    } else {
        encIn->busChromaU =   encIn->busLuma +  inputBuffer->lumaSize;
    }

    const bool is_uv_discontinuous = frame->data[1] && frame->data[2] && (frame->data[1] + inputBuffer->chromaSize/2 != frame->data[2]);
    if (is_uv_discontinuous)  {
        encIn->busChromaV = ALIGN_4K(encIn->busChromaU + inputBuffer->chromaSize / 2);
        CALC_ALIGNED_ADDR(frame->data[2], aligned_base, aligned_offset, page_mask);
        encIn->busChromaV += aligned_offset;
    } else {
        encIn->busChromaV = encIn->busChromaU + inputBuffer->chromaSize / 2;
    }
    encIn->busLumaOrig = encIn->busChromaUOrig = encIn->busChromaVOrig = (vmppDevAddr)NULL;

    // SetupExtSRAMBuffer
    encIn->extSRAMLumBwdBase = ctx->extSRAMMemFactory[ctx->pictureEncCount % ctx->parallelCoreNum].busAddress;
    encIn->extSRAMLumFwdBase = encIn->extSRAMLumBwdBase + ctx->extSramLumBwdSize;
    encIn->extSRAMChrBwdBase = encIn->extSRAMLumFwdBase + ctx->extSramLumFwdSize;
    encIn->extSRAMChrFwdBase = encIn->extSRAMChrBwdBase + ctx->extSramChrBwdSize;

    encIn->dec400Enable = 0;
    encIn->axiFEEnable = 0;
    encIn->apbFTEnable = 0;
    encIn->sceneChange = 0;
    encIn->codingType = (encIn->poc == 0) ? VCENC_INTRA_FRAME : ctx->nextCodingType;
    encIn->insertIDR = 0;
    if (inputBuffer->forceIDR) {
        encIn->codingType = VCENC_INTRA_FRAME;
        encIn->bIsIDR = 1;
        encIn->insertIDR = 1;
    }
    if (inputBuffer->number == 0 ||
               ((keyInt > 0 && (inputBuffer->number - encIn->last_idr_picture_cnt - ctx->numberBase) % keyInt == 0))) {
        if (!encIn->insertIDR) {
            if (!chn->params.videoConfig.openGop || inputBuffer->number == 0) {
                encIn->codingType = VCENC_INTRA_FRAME;
                encIn->bIsIDR = 1;
            } else {
                encIn->codingType = VCENC_CRA_FRAME;
                encIn->bIsIDR = 0;
            }
        }
        encIn->last_idr_picture_cnt = inputBuffer->number + ctx->numberBase;
    } else {
        encIn->bIsIDR = encIn->codingType == VCENC_INTRA_FRAME;
    }

    if (inputBuffer->updateKeyInt) {
        chn->params.videoConfig.keyInt = inputBuffer->updateKeyInt;
        encIn->gopConfig.idr_interval = chn->params.videoConfig.keyInt;
        if (inputBuffer->forceIDR == 2) {
            encIn->insertIDR = 0;
            encIn->poc = 0;
            encIn->gopPicIdx = 0;
            encIn->last_idr_picture_cnt = encIn->picture_cnt;
        }
    }

    if (encIn->insertIDR) {
        encIn->poc = 0;
        encIn->gopPicIdx = 0;
        /* GDR is not provided by this library version. */
        if (ctx->gdrDuration)
            LOG_DEBUG(ENC, "GDR is not supported, gdrDuration %d", ctx->gdrDuration);
    }

    encIn->bSkipFrame = 0;
    encIn->resendPPS = encIn->codingType == VCENC_INTRA_FRAME;
    encIn->resendSPS = encIn->codingType == VCENC_INTRA_FRAME;
    encIn->resendVPS = encIn->codingType == VCENC_INTRA_FRAME;

    // Setup ROI map delta QP memory
    if (inputBuffer->roiType == vmpp_ENC_ROI_MAP ||
        (chn->params.videoConfig.lookaheadDepth && (IS_HEVC(ctx->codecFormat) || IS_AV1(ctx->codecFormat)))) {
        encIn->roiMapDeltaQpAddr = inputBuffer->roiMapDeltaQpMem->busAddress;
        encIn->RoimapCuCtrlAddr = inputBuffer->roimapCuCtrlInfoMem->busAddress;
        encIn->RoimapCuCtrlIndexAddr = inputBuffer->roimapCuCtrlIndexMem->busAddress;
    }
#ifdef ROIMAP_4_HEVC2PASS_WORKAROUND
    if (chn->params.videoConfig.lookaheadDepth && IS_HEVC(ctx->codecFormat)) {
        encIn->roiMapDeltaQpAddr = ctx->roiMapDeltaQpMemFactory[ctx->pictureEncCount % ctx->bufferCnt].busAddress;
        encIn->RoimapCuCtrlAddr = (vmppDevAddr)NULL;
        encIn->RoimapCuCtrlIndexAddr = (vmppDevAddr)NULL;
    }
#endif

    if (inputBuffer->roiType == vmpp_ENC_ROI_RANGE && isROIChanged(inputBuffer->roi, ctx->lastROI)) {
        ret = setupROI(ctx, chn, inputBuffer, inputBuffer->roi);
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR(ENC, "setupROI failed: %d, encode continue with no ROI.", ret);
            // return ret;
        }
        memcpy(ctx->lastROI, inputBuffer->roi, sizeof(inputBuffer->roi));
    }

    if (!IS_AV1(ctx->codecFormat) && inputBuffer->extSEICount) {
        encIn->externalSEICount = inputBuffer->extSEICount;
        encIn->pExternalSEI = inputBuffer->extSEI;

        /* backup */
        uint32_t outBufSize = encIn->outBufSize[0];
#if defined(ANDROID_32BIT) || defined(X86_32)
        uint32_t *pOutBuf = encIn->pOutBuf[0].LSB;
#else
        uint32_t *pOutBuf = encIn->pOutBuf[0];
#endif

        encIn->outBufSize[0] = inputBuffer->encodedSEIBufferSize;
#if defined(ANDROID_32BIT) || defined(X86_32)
        encIn->pOutBuf[0].LSB = (uint32_t *)inputBuffer->encodedSEI;
#else
        encIn->pOutBuf[0] = (uint32_t *)inputBuffer->encodedSEI;
#endif

        /* The prefix and suffix SEI NAL units are not created by a separate
           call in this library version, the external SEI is handed over to
           VCEncStrmEncode() with VCEncIn.pExternalSEI. */
        inputBuffer->prefixSeiSize = 0;
        inputBuffer->suffixSeiSize = 0;

        /* restore */
        encIn->outBufSize[0] = outBufSize;
#if defined(ANDROID_32BIT) || defined(X86_32)
        encIn->pOutBuf[0].LSB = pOutBuf;
#else
        encIn->pOutBuf[0] = pOutBuf;
#endif
        encOut->streamSize = 0;
    }
    encIn->externalSEICount = 0;
    encIn->pExternalSEI = NULL;

    // update width & height for dynamic resolution
    ctx->lastEncDummyFrame.width = inputBuffer->width;
    ctx->lastEncDummyFrame.height = inputBuffer->height;

    /* The presentation time stamp is not part of VCEncIn in this version. */
    inputBuffer->svcTemporalId = (encIn->bIsIDR || encIn->insertIDR) ? 0 : encIn->gopCurrPicConfig.temporalId;

    return vmpp_RSLT_OK;
}

static vmppResult handleUpdate(struct va_enc_channel *chn, EncInputBuffer *inputBuffer)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    vmppResult ret;
    uint32_t resetRcFlag = 0;
    encVideoConfiguration videoConfigOrg = chn->params.videoConfig;

    if ((inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_CBR) &&
        ((inputBuffer->updateBitRate != 0 && inputBuffer->updateBitRate > 10000 &&
             inputBuffer->updateBitRate < (800000 * 1000)) ||
            (inputBuffer->updateVbvBufSize != 0 && inputBuffer->updateVbvBufSize > 10000 &&
                inputBuffer->updateVbvBufSize < (800000 * 1000)) ||
            (inputBuffer->updateVbvMaxRate != 0 && inputBuffer->updateVbvMaxRate > 10000 &&
                inputBuffer->updateVbvMaxRate < (800000 * 1000)))) {
        if (chn->params.videoConfig.bitRate != inputBuffer->updateBitRate ||
            chn->params.videoConfig.vbvBufSize != inputBuffer->updateVbvBufSize ||
            chn->params.videoConfig.vbvMaxRate != inputBuffer->updateVbvMaxRate) {
            chn->params.videoConfig.bitRate = inputBuffer->updateBitRate;
            chn->params.videoConfig.vbvBufSize = inputBuffer->updateVbvBufSize;
            chn->params.videoConfig.vbvMaxRate = inputBuffer->updateVbvMaxRate;
            chn->params.videoConfig.crf = -1;
            LOG_INFO(ENC, "update bitrate %d bps, vbvBufSize %d bits, vbvMaxRate %d bps", inputBuffer->updateBitRate,
                inputBuffer->updateVbvBufSize, inputBuffer->updateVbvMaxRate);
            resetRcFlag = 1;
        }
    }
    if ((inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_FRAMERATE) &&
        (inputBuffer->updateFrameRate.numerator != 0 && inputBuffer->updateFrameRate.denominator != 0) &&
        (inputBuffer->updateFrameRate.numerator != chn->params.videoConfig.frameRate.numerator ||
            inputBuffer->updateFrameRate.denominator != chn->params.videoConfig.frameRate.denominator)) {
        chn->params.videoConfig.frameRate = inputBuffer->updateFrameRate;
        LOG_INFO(ENC, "update framerate numerator %d, update framerate denominator %d.",
            inputBuffer->updateFrameRate.numerator, inputBuffer->updateFrameRate.denominator);
        resetRcFlag = 1;
    }

    inputBuffer->updateCrf = MIN(MAX(-1, inputBuffer->updateCrf), 51);
    if ((inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_CRF) && inputBuffer->updateCrf >= 0 &&
        inputBuffer->updateCrf <= 51) {
        if (chn->params.videoConfig.crf < 0) {
            LOG_INFO(ENC, "not support update crf from non-crf mode.");
        } else if (chn->params.videoConfig.crf != inputBuffer->updateCrf ||
                   chn->params.videoConfig.vbvMaxRate != inputBuffer->updateVbvMaxRate ||
                   chn->params.videoConfig.vbvBufSize != inputBuffer->updateVbvBufSize) {
            chn->params.videoConfig.crf = inputBuffer->updateCrf;
            chn->params.videoConfig.vbvBufSize = inputBuffer->updateVbvBufSize;
            chn->params.videoConfig.vbvMaxRate = inputBuffer->updateVbvMaxRate;
            LOG_INFO(ENC, "update crf %d,  vbvMaxRate %d bits, vbvMaxRate %d bps.", inputBuffer->updateCrf,
                inputBuffer->updateVbvBufSize, inputBuffer->updateVbvMaxRate);
            resetRcFlag = 1;
        }
    }

    if (inputBuffer->updateTypeMask & VMPP_ENC_UPDATE_QP) {
        if (inputBuffer->updateInitQp != VMPP_ENC_DEFAULT_PAR && chn->params.videoConfig.initQp != inputBuffer->updateInitQp) {
            chn->params.videoConfig.initQp = MIN(51, inputBuffer->updateInitQp);
            resetRcFlag = 1;
        }
        if (inputBuffer->updateQpMinI != VMPP_ENC_DEFAULT_PAR && chn->params.videoConfig.qpMinI != inputBuffer->updateQpMinI) {
            chn->params.videoConfig.qpMinI = MIN(51, inputBuffer->updateQpMinI);
            resetRcFlag = 1;
        }
        if (inputBuffer->updateQpMaxI != VMPP_ENC_DEFAULT_PAR && chn->params.videoConfig.qpMaxI != inputBuffer->updateQpMaxI) {
            chn->params.videoConfig.qpMaxI = MIN(51, inputBuffer->updateQpMaxI);
            resetRcFlag = 1;
        }
        if (inputBuffer->updateQpMinPB != VMPP_ENC_DEFAULT_PAR && chn->params.videoConfig.qpMinPB != inputBuffer->updateQpMinPB) {
            chn->params.videoConfig.qpMinPB = MIN(51, inputBuffer->updateQpMinPB);
            resetRcFlag = 1;
        }
        if (inputBuffer->updateQpMaxPB != VMPP_ENC_DEFAULT_PAR && chn->params.videoConfig.qpMaxPB != inputBuffer->updateQpMaxPB) {
            chn->params.videoConfig.qpMaxPB = MIN(51, inputBuffer->updateQpMaxPB);
            resetRcFlag = 1;
        }
        if (resetRcFlag) {
            LOG_INFO(ENC, "update initQp %d, qpMinI %d, qpMaxI %d, qpMinPB %d, qpMaxPB %d.", chn->params.videoConfig.initQp,
                chn->params.videoConfig.qpMinI, chn->params.videoConfig.qpMaxI, chn->params.videoConfig.qpMinPB, chn->params.videoConfig.qpMaxPB);
        }

    }

    if (resetRcFlag) {
        ret = resetRateCtrl(ctx, chn, &(chn->params));
        if (ret != vmpp_RSLT_OK) {
            LOG_ERROR(ENC, "resetRateCtrl error, update ratecontrol params failed.");
            chn->params.videoConfig = videoConfigOrg;
        }
    }
    return vmpp_RSLT_OK;
}

static void readCuInfo(struct va_enc_channel *chn,  u8 *cuInfoVirAddr, vmppEncOutData *cuOutData)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncOut *pEncOut = ctx->encOut;
    u32 log2_ctu_size = (IS_H264(ctx->codecFormat) ? 4 : 6);
    u32 ctu_size = (1 << log2_ctu_size);
    u32 cuInfoTotalSize = getCuInfoTotalSize(ctx, ctx->cfg.width, ctx->cfg.height);
    u32 cudataOffset = (pEncOut->cuOutData.cuData - (u8 *)pEncOut->cuOutData.ctuOffset);

    cuOutData->cuInfoVersion = 1;
    if (cuOutData->cuInfoVersion == 2) {
        ctu_size = 16;
        cuOutData->ctuPerRow = (((((ctx->cfg.width + 15) >> 4) << 4)) / ctu_size);
        cuOutData->ctuPerCol = (((((ctx->cfg.height + 15) >> 4) << 4)) / ctu_size);
        cuOutData->maxCuNum = cuOutData->ctuPerRow * cuOutData->ctuPerCol;
        cuOutData->cuData = cuInfoVirAddr + cudataOffset;
        cuOutData->cuDataTotalSize = cuInfoTotalSize - cudataOffset;
    } else {
        cuOutData->ctuPerRow = ((ctx->cfg.width  + ctu_size - 1) / ctu_size);
        cuOutData->ctuPerCol = ((ctx->cfg.height + ctu_size - 1) / ctu_size);
        cuOutData->maxCuNum = cuOutData->ctuPerRow * cuOutData->ctuPerCol * (ctu_size / 8) * (ctu_size / 8);
        cuOutData->cuData = cuInfoVirAddr;
        cuOutData->cuDataTotalSize = cuInfoTotalSize;
    }

    /* The CU information buffer is mapped into the CPU address space, the data
       is copied instead of being transferred by a DMA engine. */
    if (cuInfoVirAddr != NULL && pEncOut->cuOutData.ctuOffset != NULL)
        memcpy(cuInfoVirAddr, (const void *)pEncOut->cuOutData.ctuOffset, cuInfoTotalSize);
}

static vmppResult handleOutput(
    struct va_enc_channel *chn, vmppStream *stream, EncInputBuffer *inputBuffer, EWLLinearMem_t *outputBuffer)
{
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    EncInputBuffer *temp;
    int inputIndex = 0;
    u8 *av1Header = NULL;
    outputBuffer = getReadyOutputBuffer(ctx);
    temp = (ctx->flushing || ctx->internalFlushing) ? NULL : inputBuffer;
    if (ctx->workmode == MULTI_CORE_MODE) {
        int32_t expNum = ctx->internalFlushing ? ctx->firstFrameNumberOfNewRes : encOut->picture_cnt + ctx->numberBase;
        getInputBuffer(ctx, expNum, &temp);
    }

    if (chn->params.videoConfig.lookaheadDepth) {
        inputIndex = getInputBuffer(ctx, encOut->picture_cnt + ctx->numberBase, &temp);
        if (inputIndex < 0) {
            LOG_ERROR(ENC,
                "Error buffer state: encOut->picture_cnt %d, encIn->picture_cnt %d, "
                "inputIndex %d",
                encOut->picture_cnt, encIn->picture_cnt, inputIndex);
            assert(0);
        }
    }

    LOG_DEBUG(ENC,
        "DEBUG: -Output- encOut->picture_cnt %d, encIn->picture_cnt %d, "
        "inputIndex %d, encOut->codingType %d, NUMBER %d",
        encOut->picture_cnt, encIn->picture_cnt, inputIndex, encOut->codingType, temp->number);

    uint32_t offset = 0;
    stream->encedNals.cnt = 0;
    if (!IS_AV1(ctx->codecFormat) && (!ctx->parametersSetOutputed || encOut->codingType == VCENC_INTRA_FRAME)) {
        LOG_DEBUG(ENC,
            "DEBUG: -SPS-PPS-VPS- encOut->picture_cnt %d, encIn->picture_cnt %d, "
            "inputIndex %d, encOut->codingType %d, NUMBER %d",
            encOut->picture_cnt, encIn->picture_cnt, inputIndex, encOut->codingType, temp->number);
        memcpy(outputBuffer->virtualAddress, ctx->parametersSet, ctx->parametersSetSize);

        if (IS_HEVC(ctx->codecFormat)) {
            // VPS + SPS + PPS
            stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_VPS;
            stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_SPS;
            stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_PPS;
        } else if (IS_H264(ctx->codecFormat)) {
            if (ctx->cfg.maxTLayers > 1) {
                // SVC-T SEI
                stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_PREFIX_SEI;
            }
            // SPS + PPS
            stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_SPS;
            stream->encedNals.nals[stream->encedNals.cnt++] = vmpp_NAL_PPS;
        } else {
            stream->encedNals.cnt = 0;
        }

        offset = ctx->parametersSetSize;
        if (!ctx->parametersSetOutputed) {
            ctx->parametersSetOutputed = 1;
        }
    }

    if (IS_AV1(ctx->codecFormat) && (!ctx->ivfHeaderOutputed || encOut->picture_cnt == 0)) {
        LOG_DEBUG(ENC,
            "DEBUG: -IVFHEADER- encOut->picture_cnt %d, encIn->picture_cnt %d, "
            "inputIndex %d, encOut->codingType %d, NUMBER %d",
            encOut->picture_cnt, encIn->picture_cnt, inputIndex, encOut->codingType, temp->number);
        memcpy(outputBuffer->virtualAddress, ctx->ivfHeader, ctx->ivfHeaderSize);
        offset = ctx->ivfHeaderSize;
        if (!ctx->ivfHeaderOutputed) {
            ctx->ivfHeaderOutputed = 1;
        }
        stream->encedNals.cnt = 1;
        stream->encedNals.nals[0] = vmpp_NAL_IVF_HEADER;
    }

    if (!IS_AV1(ctx->codecFormat) && temp->extSEICount && temp->prefixSeiSize) {
        memcpy(((uint8_t *)outputBuffer->virtualAddress + offset), temp->encodedSEI, temp->prefixSeiSize);
        offset += temp->prefixSeiSize;
        stream->encedNals.cnt++;
        stream->encedNals.nals[stream->encedNals.cnt - 1] = vmpp_NAL_PREFIX_SEI;
    }

    if (IS_AV1(ctx->codecFormat)) {
        if (ctx->workmode == MULTI_CORE_MODE) {
            av1Header = getReadyAv1HeaderOutputBuffer(ctx);
            if (!av1Header) {
                LOG_ERROR(ENC, "No available av1Header buffer.");
                return vmpp_RSLT_ERR_NO_BUFFER;
            }
        }
        generateAV1stream(chn, encOut, outputBuffer, &offset);
    } else {
        /* The stream buffer is mapped into the CPU address space, the hardware
           output only has to be made visible for the CPU. */
        DMATransRead(outputBuffer, 0, encOut->streamSize,
                     (uint8_t *)outputBuffer->virtualAddress + offset);
        offset += encOut->streamSize;
        stream->encedNals.cnt++;
        switch (encOut->codingType) {
        case VCENC_INTRA_FRAME:
            stream->encedNals.nals[stream->encedNals.cnt - 1] = vmpp_NAL_I;
            break;
        case VCENC_PREDICTED_FRAME:
            stream->encedNals.nals[stream->encedNals.cnt - 1] = vmpp_NAL_P;
            break;
        case VCENC_BIDIR_PREDICTED_FRAME:
            stream->encedNals.nals[stream->encedNals.cnt - 1] = vmpp_NAL_B;
            break;
        default:
            break;
        }
    }

    if (!IS_AV1(ctx->codecFormat) && temp->extSEICount && temp->suffixSeiSize) {
        memcpy(((uint8_t *)outputBuffer->virtualAddress + offset), temp->encodedSEI + temp->prefixSeiSize,
            temp->suffixSeiSize);
        offset += temp->suffixSeiSize;
        stream->encedNals.cnt++;
        stream->encedNals.nals[stream->encedNals.cnt - 1] = vmpp_NAL_SUFFIX_SEI;
    }
    /* The HRD filler data is not supported by this library version. */

    stream->stream = (uint8_t *)outputBuffer->virtualAddress;
    stream->len = offset;
    stream->pts = temp->pts;
    stream->svcTemporalId = temp->svcTemporalId;
    stream->inputBusAddress = temp->memType == vmpp_MEM_DEVICE ? temp->mem.busAddress : (vmppDevAddr)NULL;
    ctx->outputPictureCount++;
    if (temp->roiMapDeltaQpMem) {
        setROIMemIdle(ctx, temp->roiMapDeltaQpMem);
    }

#if 0
            int lum_max_value, cbcr_max_value;
            lum_max_value = (1 << ctx->cfg.bitDepthLuma) - 1;
            cbcr_max_value = (1 << ctx->cfg.bitDepthChroma) - 1;

            double y_psnr, cb_psnr, cr_psnr;
            y_psnr  =  10.0 * log10f(lum_max_value * lum_max_value /  encOut->psnr[0]);  // encOut->psnr[0]: lum_mse
            cb_psnr =  10.0 * log10f(cbcr_max_value * cbcr_max_value / encOut->psnr[1]); // encOut->psnr[1]: cb_mse
            cr_psnr =  10.0 * log10f(cbcr_max_value * cbcr_max_value / encOut->psnr[2]); // encOut->psnr[2]: cr_mse

            ctx->psnr_total[0] += y_psnr;
            ctx->psnr_total[1] += cb_psnr;
            ctx->psnr_total[2] += cr_psnr;
            LOG_ERROR(ENC, "encOut->psnr %4.2f %4.2f %4.2f", encOut->psnr[0], encOut->psnr[1], encOut->psnr[2]);
            LOG_ERROR(ENC, "ctx->psnr_total %4.2f %4.2f %4.2f", ctx->psnr_total[0], ctx->psnr_total[1], ctx->psnr_total[2]);
#endif
    stream->psnrInfo[0] = encOut->psnr[0];
    stream->psnrInfo[1] = encOut->psnr[1];
    stream->psnrInfo[2] = encOut->psnr[2];
    stream->psnrInfo[3] = encOut->ssim[0];
    stream->psnrInfo[4] = encOut->ssim[1];
    stream->psnrInfo[5] = encOut->ssim[2];

    if (chn->params.videoConfig.enableOutputCuInfo)
        readCuInfo(chn, (uint8_t *)outputBuffer->virtualAddress + temp->orgStreamSize, &stream->encOutData);

    stream->encOutData.frameType = (vmppFrameType)encOut->codingType;
    /* The average QP of a frame is not provided by this library version. */
    stream->encOutData.frameAvgQP = 0;

    setInputBufferIdle(ctx, temp);
    return vmpp_RSLT_OK;
}

vmppResult video_multicore_flush(struct va_enc_channel *chn, EncInputBuffer **inputBuffer, vmppStream *stream)
{
    VCEncRet vRet;
    EWLLinearMem_t *outputBuffer = NULL;
    struct video_encoder_private_context *ctx =
        (struct video_encoder_private_context *)chn->private_context;
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;

    vRet = VCEncFlush(chn->codec_inst, encIn, encOut, NULL, NULL);
    if (vRet == VCENC_FRAME_READY) {
        ctx->pictureEncCount++;
        handleOutput(chn, stream, *inputBuffer, outputBuffer);
    }
    return vmpp_RSLT_OK;
}

vmppResult video_encode_frame(struct va_enc_channel *chn, vmppFrame *frame, vmppEncExtendedParams *extParams,
    vmppStream *stream, uint32_t timeout)
{
    vmppResult ret;
    VCEncRet vRet;
    int inputIndex = 0;
    int frameSaved = 0;
    int newEncoder = 0;
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    uint32_t adaptiveGop = (chn->params.videoConfig.gopSize == 0);
    VCEncIn *encIn = ctx->encIn;
    VCEncOut *encOut = ctx->encOut;
    EWLLinearMem_t *outputBuffer = NULL;
    EncInputBuffer *inputBuffer = NULL;

    ret = checkParamsAndStatus(ctx, frame);
    if (ret != vmpp_RSLT_OK) {
        return ret;
    }

    ret = preProcInputAndOutput(chn, frame, extParams, &inputBuffer, &outputBuffer, &frameSaved, timeout);
    if (ret != vmpp_RSLT_OK) {
        return ret;
    }

    ret = updateGopConfig(chn, frame, extParams);
    if (ret != vmpp_RSLT_OK) {
        return ret;
    }

    while (HANTRO_TRUE) {
        /* prepare inputBuffer */
        int32_t expNum = ctx->internalFlushing ? ctx->firstFrameNumberOfNewRes : encIn->picture_cnt + ctx->numberBase;
        inputIndex = getInputBuffer(ctx, expNum, &inputBuffer);

        LOG_DEBUG(ENC,
            "getInputBuffer: encIn->picture_cnt %d, inputIndex %d, "
            "chn->params.videoConfig.P2B %d, ctx->numberBase %lld",
            encIn->picture_cnt, inputIndex, chn->params.videoConfig.P2B, (U64)ctx->numberBase);

        /* insert IDR gop change */
        if (ctx->encInLast.picture_cnt + ctx->numberBase < ctx->insertIdrPicCnt &&
            ctx->nextCodingType != VCENC_INTRA_FRAME) {
            video_insert_idr_gopchange(chn, &inputBuffer, &inputIndex);
        }

        /* handle dynamic resolution */
        ctx->internalFlushing = 0;
        if (inputIndex >= 0 && inputBuffer->newResolution) {
            uint32_t need_flush = getBufferCntNeed2Flush(ctx, inputBuffer->number);
            LOG_DEBUG(ENC,
                "Reach the first frame of new resolution: REMAIN BUFFER %d,"
                "inputBuffer->number %d, need flush number %d",
                getNotEncodedBufferCnt(ctx), inputBuffer->number, need_flush);
            ret = handle_dynamic_resolution(chn, inputBuffer, outputBuffer, &newEncoder, need_flush);
            if (ret != vmpp_RSLT_OK) {
                return ret;
            }
            if (need_flush > 0) {
                if (ctx->workmode == MULTI_CORE_MODE &&
                    chn->params.videoConfig.lookaheadDepth == 0) {
                    return video_multicore_flush(chn, &inputBuffer, stream);
                }
            }
        }

        /* handle EOS */
        if (inputIndex < 0) {
            LOG_DEBUG(ENC,
                "DEBUG: ctx->outputPictureCount %d, "
                "ctx->inputPictureCount %d, getNotEncodedBufferCnt(ctx) %d",
                ctx->outputPictureCount, ctx->inputPictureCount, getNotEncodedBufferCnt(ctx));
            if (frame->memoryType == vmpp_MEM_FLUSH) {
                int remainJobs = 0;
                ret = handleFlush(chn, stream, outputBuffer, &inputBuffer, &inputIndex, &remainJobs);
                if (!remainJobs) {
                    return ret;
                }
            } else {
                set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
                ctx->currInsertedNum++;
                LOG_DEBUG(ENC, "Input inserted2! (%d)", ctx->currInsertedNum);
                return vmpp_RSLT_ENC_INPUT_INSERTED;
            }
        }

        /* prepare encIn from inputBuffer */
        if (!ctx->flushing && !ctx->internalFlushing) {
            finalizeVCEncIn(chn, inputBuffer, frame);
            if (inputBuffer->updateTypeMask) {
                handleUpdate(chn, inputBuffer);
            }
        }

        /* do encoding & output */
        vRet = VCEncStrmEncode(chn->codec_inst, (ctx->flushing || ctx->internalFlushing) ? NULL : encIn, encOut,
            NULL /*&HEVCSliceReady*/, NULL);

        ctx->lastVRet = vRet;

        if (!ctx->flushing && !ctx->internalFlushing) {
            // for insertIDR
            ctx->encInLast = *encIn;

            if (inputBuffer->gopChangeIdr && encIn->codingType == VCENC_INTRA_FRAME) {
                ctx->nextGopSize = (adaptiveGop ? 4 : chn->params.videoConfig.gopSize);
                encIn->gopSize = ctx->nextGopSize;
                inputBuffer->forceIDR = 0;
            }
            inputBuffer->gopChangeIdr = 0;
        }

        switch (vRet) {
        case VCENC_FRAME_ENQUEUE:
            if (!ctx->flushing && !ctx->internalFlushing) {
                inputBuffer->sent2Encoder = 1;
            }
            ctx->pictureEncCount++;
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            ctx->currInsertedNum++;
            /* The picture number belongs to this layer: pictures are handed over one by one,
             * the internal single pass state advances on its own in lockstep with this one.
             * Without the increment every picture would enter the job queue with the same
             * picture_cnt and no job could ever be scheduled for encoding. */
            encIn->picture_cnt++;
            encIn->picture_gopIdx++;
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            LOG_DEBUG(ENC, "input inserted1! (%d)", ctx->currInsertedNum);
            if (!frameSaved && (ctx->internalFlushing || newEncoder)) {
                return vmpp_RSLT_ENC_AGAIN_WITH_NO_OUTPUT;
            }
            return vmpp_RSLT_ENC_INPUT_INSERTED;
        case VCENC_FRAME_READY:
            ctx->currInsertedNum = 0;
            if (!ctx->flushing && !ctx->internalFlushing) {
                inputBuffer->sent2Encoder = 1;
            }

            if (encOut->codingType != VCENC_NOTCODED_FRAME) {
                ctx->pictureEncCount++;
            }

            if (encOut->streamSize == 0) {
                LOG_WARN(ENC, "encOut->streamSize == 0");
            } else {
                handleOutput(chn, stream, inputBuffer, outputBuffer);
            }

            /* See the comment in the VCENC_FRAME_ENQUEUE case: picture_cnt has to point at
             * the next picture, also when nothing was written into the stream. */
            encIn->picture_cnt++;
            encIn->picture_gopIdx++;
            encIn->timeIncrement = chn->params.videoConfig.frameRate.denominator;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
        default:
            if (vRet == VCENC_HASH_LEN_MISMATCH) {
                ctx->hashLenMismatch = 1;
                return vmpp_RSLT_ERR_ENC_DRIVER_MISMATCH;
            }
            if (vRet == VCENC_OK && ctx->workmode == MULTI_CORE_MODE &&
                (ctx->flushing || ctx->internalFlushing) &&
                chn->params.videoConfig.lookaheadDepth > 0) {
                return video_multicore_flush(chn, &inputBuffer, stream);
            }
            set_out_buffer_idle(chn, (uint8_t *)outputBuffer->virtualAddress);
            LOG_ERROR(ENC, "VCEncStrmEncode ERROR: %d", vRet);
            return vmpp_RSLT_ERR_ENC_SEND_FRAME;
        }
        break;
    }
    if (!frameSaved && (ctx->internalFlushing || newEncoder)) {
        return vmpp_RSLT_ENC_AGAIN;
    }
    return vmpp_RSLT_OK;
}

vmppResult video_encoder_release_stream(struct va_enc_channel *chn, vmppStream *stream)
{
    uint8_t *tmp;
    vmppResult ret = vmpp_RSLT_OK;

    tmp = (uint8_t *)stream->stream;
    ret = set_out_buffer_idle(chn, tmp);

    return ret;
}

vmppResult video_parse_cu_info(struct va_enc_channel *chn, vmppEncOutData *cuOutData, vmppEncOutInfo *encOutInfo)
{
    vmppResult ret = vmpp_RSLT_OK;
    VCEncInst encoder = (VCEncInst)chn->codec_inst;
    struct video_encoder_private_context *ctx = (struct video_encoder_private_context *)chn->private_context;
    VCEncCuOutData EncCuOutData;
    VCEncCuInfo currentCuInfo = {0};
    vmppEncCuInfo *userCuinfo = NULL;
    uint32_t iCtuX, iCtuY, iCtu = 0, iCu = 0, cuNum = 0;
    int64_t mvXTotal = 0, mvYTotal = 0;

    if (!encOutInfo || !encOutInfo->cuInfo) {
        LOG_ERROR(ENC, "video_parse_cu_info error, invalid encInfo!!!\n");
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    EncCuOutData.ctuOffset = (uint32_t *)cuOutData->cuData;//for version 1
    EncCuOutData.cuData = cuOutData->cuData;
    uint32_t * ctuTable = EncCuOutData.ctuOffset;

    encOutInfo->frameSatd = 0;
    encOutInfo->averageMVX = encOutInfo->averageMVY = 0;
    encOutInfo->interMBCount = encOutInfo->intraMBCount = 0;

    for (iCtuY = 0; iCtuY < cuOutData->ctuPerCol; iCtuY ++) {
        for (iCtuX = 0; iCtuX < cuOutData->ctuPerRow; iCtuX ++) {
            uint32_t nCu = 1;
            if (IS_H264(ctx->codecFormat) || cuOutData->cuInfoVersion == 2) {
                nCu = 1;
            } else if (ctuTable) {
                nCu = ctuTable[iCtu];
                if (iCtu)
                    nCu -= ctuTable[iCtu - 1];
            }

            for (iCu = 0; iCu < nCu; iCu++) {
                /* The CU information of version 2 is read with the generic
                   interface, this library version has no VCEncGetCuInfo_V2(). */
                VCEncGetCuInfo(encoder, &EncCuOutData, iCtu, iCu, &currentCuInfo);

                userCuinfo = encOutInfo->cuInfo + cuNum;
                userCuinfo->cuLocationX   = currentCuInfo.cuLocationX;
                userCuinfo->cuLocationY   = currentCuInfo.cuLocationY;
                userCuinfo->cuSize        = currentCuInfo.cuSize;
                userCuinfo->cuMode        = currentCuInfo.cuMode;
                userCuinfo->costIntraSatd = currentCuInfo.costIntraSatd;
                userCuinfo->costInterSatd = currentCuInfo.costInterSatd;
                userCuinfo->interPredIdc  = currentCuInfo.interPredIdc;

                memcpy(&userCuinfo->mv, &currentCuInfo.mv, sizeof(vmppEncMv) * 2);

                if (currentCuInfo.cuMode == 1) {
                    encOutInfo->intraMBCount++;
                    encOutInfo->frameSatd += currentCuInfo.costIntraSatd;
                } else {
                    encOutInfo->interMBCount++;
                    encOutInfo->frameSatd += currentCuInfo.costInterSatd;
                }
                if (currentCuInfo.interPredIdc == 2) {
                    mvXTotal += (currentCuInfo.mv[0].mvX + currentCuInfo.mv[1].mvX) >> 1;
                    mvYTotal += (currentCuInfo.mv[0].mvY + currentCuInfo.mv[1].mvY) >> 1;
                } else if (currentCuInfo.interPredIdc == 1) {
                    mvXTotal += currentCuInfo.mv[1].mvX;
                    mvYTotal += currentCuInfo.mv[1].mvY;
                } else {
                    mvXTotal += currentCuInfo.mv[0].mvX;
                    mvYTotal += currentCuInfo.mv[0].mvY;
                }
                cuNum++;
            }
            iCtu++;
        }
    }

    if (encOutInfo->interMBCount) {
        encOutInfo->averageMVX = mvXTotal / encOutInfo->interMBCount;
        encOutInfo->averageMVY = mvYTotal / encOutInfo->interMBCount;
    }
    encOutInfo->totalCuNum = cuNum;
    return ret;
}

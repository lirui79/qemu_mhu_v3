#ifndef SEI_DECODER_H
#define SEI_DECODER_H
#include "va_vdata_internal.h"
#include "vmpp_dec_defs.h"

typedef enum {
    NAL_UNSPECIFIED = 0,
    NAL_CODED_SLICE = 1,
    NAL_CODED_SLICE_DP_A = 2,
    NAL_CODED_SLICE_DP_B = 3,
    NAL_CODED_SLICE_DP_C = 4,
    NAL_CODED_SLICE_IDR = 5,
    NAL_SEI = 6,
    NAL_SEQ_PARAM_SET = 7,
    NAL_PIC_PARAM_SET = 8,
    NAL_ACCESS_UNIT_DELIMITER = 9,
    NAL_END_OF_SEQUENCE = 10,
    NAL_END_OF_STREAM = 11,
    NAL_FILLER_DATA = 12,
    NAL_SPS_EXT = 13,
    NAL_PREFIX = 14,
    NAL_SUBSET_SEQ_PARAM_SET = 15,
    NAL_CODED_SLICE_AUX = 19,
    NAL_CODED_SLICE_EXT = 20,
    NAL_MAX_TYPE_VALUE = 31
} NAL_TYPE;

void free_sei_parameter(struct va_dec_channel *chn);
uint32_t get_sei_parameter_for_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                     int64_t extra_pts);
void set_sei_parameter_idle_frame(struct va_dec_channel *chn, vmppFrame *frame);

vmppResult sei_decoder(struct va_dec_channel *chn, vmppStream *stream, uint64_t pts);

#endif
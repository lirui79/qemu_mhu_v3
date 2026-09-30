#include "sei_decoder.h"
#include "h264decapi.h"
#include "va_log.h"

#define BUFFER_ALIGN_MASK 0xF

typedef struct {
    int forbidden_zero_bit;
    int nal_ref_idc;
    int nal_unit_type;
} nal_unit;

typedef struct {
    int forbidden_zero_bit;
    int nal_unit_type;
    int nuh_reserved_zero_6bits;
    int nuh_temporal_id_plus1;
} hevc_nal_unit;

struct va_sei_buf *get_sei_idle_buffer(struct va_dec_channel *chn, uint32_t require_length);
void set_sei_idle_buffer(struct va_dec_channel *chn, void *sei_data);
void clear_sei_buffer(struct va_dec_channel *chn);

vmppSEI *get_idle_sei_parameter(struct va_dec_channel *chn, uint64_t pts);
void set_sei_parameter_idle_data(struct va_dec_channel *chn, vmppSEI *data);

//----------------common function begin: bit_stream---------------//
typedef struct {
    uint8_t *start;
    uint8_t *p;
    uint8_t *end;
    int bits_left;
} bit_stream;

void bs_init(bit_stream *bs, uint8_t *buf, int size)
{
    bs->start = buf;
    bs->p = buf;
    bs->end = buf + size;
    bs->bits_left = 8;
}

bit_stream *bs_new(struct va_dec_channel *chn, uint8_t *buf, size_t size)
{
    struct va_sei_buf *sei_buf = get_sei_idle_buffer(chn, sizeof(bit_stream));
    if (!sei_buf) {
        return NULL;
    }

    bit_stream *bs = (bit_stream *)sei_buf->data;
    bs_init(bs, buf, size);
    return bs;
}

void bs_free(struct va_dec_channel *chn, bit_stream *bs)
{
    if (!chn || !bs) {
        return;
    }
    set_sei_idle_buffer(chn, bs);
}

uint32_t bs_byte_aligned(bit_stream *bs)
{
    if (bs->bits_left == 8) {
        return 1;
    } else {
        return 0;
    }
}

uint32_t bs_eof(bit_stream *bs)
{
    if (bs->p >= bs->end) {
        return 1;
    } else {
        return 0;
    }
}

int bs_pos(bit_stream *bs) { return (bs->p - bs->start); }

uint32_t bs_read_u1(bit_stream *bs)
{
    uint32_t r = 0;
    if (bs_eof(bs)) {
        return 0;
    }

    bs->bits_left--;
    r = ((*(bs->p)) >> bs->bits_left) & 0x01;

    if (bs->bits_left == 0) {
        bs->p++;
        bs->bits_left = 8;
    }

    return r;
}

uint32_t bs_read_u(bit_stream *bs, int n)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < n; i++) {
        r |= (bs_read_u1(bs) << (n - i - 1));
    }
    return r;
}

uint32_t bs_read_u8(bit_stream *bs) { return bs_read_u(bs, 8); }

uint32_t bs_peek_u1(bit_stream *bs)
{
    uint32_t r = 0;
    if (!bs_eof(bs)) {
        r = ((*(bs->p)) >> (bs->bits_left - 1));
        r &= 0x01;
    }
    return r;
}

int bs_more_data(bit_stream *bs)
{
    if (bs_eof(bs)) {
        return 0;
    }
    if (bs_peek_u1(bs) == 1) {
        return 0;
    }
    return 1;
}

int bs_overrun(bit_stream *b)
{
    if (b->p > b->end) {
        return 1;
    } else {
        return 0;
    }
}
//----------------common function end: bit_stream---------------//

//----------------memory management begin-------------------//
struct va_sei_buf *get_sei_idle_buffer(struct va_dec_channel *chn, uint32_t require_length)
{
    int i;
    int matched = 0;
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    // unused and size matched, break
    for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (0 == chn->sei_buffer[i].used && chn->sei_buffer[i].size >= require_length) {
            //memset(chn->sei_buffer[i].data, 0, chn->sei_buffer[i].size);
            matched = 1;
            break;
        }
    }

    // check unused but size not match
    if (!matched) {
        for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
            if (0 == chn->sei_buffer[i].used && chn->sei_buffer[i].data) {
                uint8_t *tmp = (uint8_t *)realloc(chn->sei_buffer[i].data, require_length);
                if (!tmp) {
                    pthread_mutex_unlock(&chn->sei_buffer_mutex);
                    return NULL;
                }

                chn->sei_buffer[i].data = tmp;
                //memset(chn->sei_buffer[i].data, 0, require_length);
                chn->sei_buffer[i].size = require_length;
                matched = 1;
                break;
            } else if (0 == chn->sei_buffer[i].used && !chn->sei_buffer[i].data) {
                break;
            }
        }
    }

    if (i >= VA_MAX_SEI_BUFFER) {
        LOG_WARN(DEC, "No idle sei buffer avaliable.");
        pthread_mutex_unlock(&chn->sei_buffer_mutex);
        return NULL;
    }

    if (!chn->sei_buffer[i].data) {
        chn->sei_buffer[i].data = (uint8_t *)malloc(require_length);
        //memset(chn->sei_buffer[i].data, 0, require_length);
        chn->sei_buffer[i].size = require_length;
        if (!chn->sei_buffer[i].data) {
            LOG_ERROR(DEC, "Fail to malloc sei buffer.");
            pthread_mutex_unlock(&chn->sei_buffer_mutex);
            return NULL;
        }
    }

    chn->sei_buffer[i].used = 1;
    pthread_mutex_unlock(&chn->sei_buffer_mutex);
    return &chn->sei_buffer[i];
}

void set_sei_idle_buffer(struct va_dec_channel *chn, void *sei_data)
{
    int i;
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (1 == chn->sei_buffer[i].used && chn->sei_buffer[i].data == sei_data) {
            chn->sei_buffer[i].used = 0;
            break;
        }
    }
    pthread_mutex_unlock(&chn->sei_buffer_mutex);
}

void clear_sei_buffer(struct va_dec_channel *chn)
{
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (uint32_t i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (chn->sei_buffer[i].data && chn->sei_buffer[i].size) {
            free(chn->sei_buffer[i].data);
            chn->sei_buffer[i].data = NULL;
        }
    }
    pthread_mutex_unlock(&chn->sei_buffer_mutex);
}
//----------------memory management end--------------------//

//----------------sei params cache begin-------------------//

vmppSEI *get_idle_sei_parameter(struct va_dec_channel *chn, uint64_t pts)
{
    vmppSEI *prames = NULL;
    uint32_t i = 0;
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (!chn->va_sei_parameters[i].used && !chn->va_sei_parameters[i].privateData)
            break;
    }
    if (i >= VA_MAX_SEI_BUFFER) {
        LOG_WARN(DEC, "No idle sei parameter buffer avaliable.");
        pthread_mutex_unlock(&chn->sei_buffer_mutex);
        return NULL;
    }

    if (!chn->va_sei_parameters[i].sei_data) {
        chn->va_sei_parameters[i].sei_data = (vmppSEI *)malloc(sizeof(vmppSEI));
        if (!chn->va_sei_parameters[i].sei_data) {
            LOG_WARN(DEC, "Fail to malloc sei sei_data struct.");
            pthread_mutex_unlock(&chn->sei_buffer_mutex);
            return NULL;
        }
    }
    memset(chn->va_sei_parameters[i].sei_data, 0, sizeof(vmppSEI));
    prames = chn->va_sei_parameters[i].sei_data;
    chn->va_sei_parameters[i].used = 1;
    chn->va_sei_parameters[i].pts = pts;

    pthread_mutex_unlock(&chn->sei_buffer_mutex);
    return prames;
}

uint32_t get_sei_parameter_for_frame(struct va_dec_channel *chn, vmppFrame *frame,
                                     int64_t extra_pts)
{
    frame->seiCount = 0;
    uint32_t sei_count = 0;
    uint32_t i;
    uint32_t expect_size = 0;
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (chn->va_sei_parameters[i].used &&
            (chn->va_sei_parameters[i].pts == (uint64_t)frame->pts ||
             (extra_pts >= 0 && chn->va_sei_parameters[i].pts == (uint64_t)extra_pts)) &&
            chn->va_sei_parameters[i].sei_data && !chn->va_sei_parameters[i].privateData) {
            sei_count++;
        }
    }

    if (sei_count <= 0) {
        pthread_mutex_unlock(&chn->sei_buffer_mutex);
        return -1;
    }

    expect_size = sizeof(vmppSEI *) * sei_count;
    struct va_sei_buf *sei_buf;
    sei_buf = get_sei_idle_buffer(chn, expect_size);
    if (!sei_buf || !sei_buf->data) {
        pthread_mutex_unlock(&chn->sei_buffer_mutex);
        return -1;
    }

    uint32_t sei_index = 0;
    frame->seiData = (vmppSEI **)sei_buf->data;
    for (i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (chn->va_sei_parameters[i].used &&
            (chn->va_sei_parameters[i].pts == (uint64_t)frame->pts ||
             (extra_pts > 0 && chn->va_sei_parameters[i].pts == (uint64_t)extra_pts)) &&
            chn->va_sei_parameters[i].sei_data && !chn->va_sei_parameters[i].privateData) {
            if (chn->va_sei_parameters[i].sei_data->payloadData &&
                chn->va_sei_parameters[i].sei_data->payloadDataSize) {
                chn->va_sei_parameters[i].privateData = frame->privateData;
                frame->seiData[sei_index] = chn->va_sei_parameters[i].sei_data;
                sei_index++;
            } else {
                set_sei_parameter_idle_data(chn, chn->va_sei_parameters[i].sei_data);
            }
        }
    }
    frame->seiCount = sei_index;

    if (frame->seiCount == 0)
        set_sei_idle_buffer(chn, frame->seiData);

    pthread_mutex_unlock(&chn->sei_buffer_mutex);
    return 0;
}

void set_sei_parameter_idle_frame(struct va_dec_channel *chn, vmppFrame *frame)
{
    for (size_t i = 0; i < frame->seiCount; i++) {
        set_sei_parameter_idle_data(chn, frame->seiData[i]);
    }
    if (frame->seiCount) {
        set_sei_idle_buffer(chn, frame->seiData);
    }
    frame->seiCount = 0;
}

void set_sei_parameter_idle_data(struct va_dec_channel *chn, vmppSEI *data)
{
    if (!chn || !data) {
        return;
    }

    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (uint32_t i = 0; i < VA_MAX_SEI_BUFFER; i++) {
        if (chn->va_sei_parameters[i].used && chn->va_sei_parameters[i].sei_data == data) {
            void *sei_payload_data = chn->va_sei_parameters[i].sei_data->payloadData;
            if (sei_payload_data) {
                // set data idle
                set_sei_idle_buffer(chn, sei_payload_data);
            }

            memset(chn->va_sei_parameters[i].sei_data, 0, sizeof(vmppSEI));

            chn->va_sei_parameters[i].used = 0;
            chn->va_sei_parameters[i].privateData = NULL;
            chn->va_sei_parameters[i].pts = 0;
            break;
        }
    }
    pthread_mutex_unlock(&chn->sei_buffer_mutex);
}

void free_sei_parameter(struct va_dec_channel *chn)
{
    clear_sei_buffer(chn);
    pthread_mutex_lock(&chn->sei_buffer_mutex);
    for (int i = 0; i < VA_MAX_SEI_BUFFER; i++)
        if (chn->va_sei_parameters[i].sei_data) {
            // relase internal data
            free(chn->va_sei_parameters[i].sei_data);
            chn->va_sei_parameters[i].sei_data = NULL;
        }
    memset(chn->va_sei_parameters, 0, sizeof(chn->va_sei_parameters));
    pthread_mutex_unlock(&chn->sei_buffer_mutex);
}
//----------------sei params cache end-------------------//

// return data length
int find_nal_unit(uint8_t *buf, int size, int *nal_start, int *nal_end)
{
    if (!buf || !nal_start || !nal_end || size < 4) {
        return -1;
    }
    int i = 0;
    *nal_start = 0;
    *nal_end = 0;
    while ((buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0x01) &&
           (buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0 || buf[i + 3] != 0x01)) {
        i++;
        if (i + 4 >= size) {
            return 0;
        }
    }

    if (buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0x01) {
        i++;
    }

    if (buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0x01) {
        return 0;
    }

    i += 3;
    *nal_start = i;

    if (i + 3 >= size) {
        return -1;
    }

    while ( // next 3: 00 00 00 or 00 00 01
        (buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0) &&
        (buf[i] != 0 || buf[i + 1] != 0 || buf[i + 2] != 0x01)) {
        i++;

        if (i + 3 >= size) {
            *nal_end = size - 1;
            return (*nal_end - *nal_start);
        }
    }

    *nal_end = i;
    return (*nal_end - *nal_start);
}

void read_rbsp_trailing_bits(bit_stream *bs)
{
    // int rbsp_stop_one_bit = bs_read_u1(bs); // equal to 1
    bs_read_u1(bs); // only read and move forward
    while (!bs_byte_aligned(bs)) {
        // int rbsp_alignment_zero_bit = bs_read_u1(bs); // equal to 0
        bs_read_u1(bs); // only read and move forward
    }
}

//------------------------h264 sei begin---------------------//
int h264_read_ff_coded_number(bit_stream *bs)
{
    int n1 = 0;
    int n2;
    do {
        n2 = bs_read_u8(bs);
        n1 += n2;
    } while (n2 == 0xff);
    return n1;
}

void h264_read_sei_end_bits(bit_stream *bs)
{
    if (!bs_byte_aligned(bs)) {
        if (!bs_read_u1(bs)) {
            // bit_equal_to_one is 0
        }
        while (!bs_byte_aligned(bs)) {
            if (bs_read_u1(bs)) {
                // bit_equal_to_zero is 1;
            }
        }
    }

    read_rbsp_trailing_bits(bs);
}

int h264_read_user_data_unregistered(struct va_dec_channel *chn, bit_stream *bs,
                                     uint32_t payload_size, vmppSEI *sei_params)
{
    uint32_t i;
    struct va_sei_buf *sei_buf = NULL;
    sei_buf = get_sei_idle_buffer(chn, payload_size + 1);
    if (!sei_buf) {
        LOG_ERROR(DEC, "No idle sei buffer!");
        return -1;
    }
    sei_params->payloadDataSize = payload_size;
    sei_params->payloadData = sei_buf->data;

    for (i = 0; i < 16; i++) {
        if (bs_eof(bs)) {
            LOG_ERROR(DEC, "Invalid SEI data!");
            return -1;
        }
        sei_params->payloadData[i] = bs_read_u(bs, 8);
        // bs_read_u(bs, 8);
    }
    for (i = 16; i < payload_size; i++) {
        if (bs_eof(bs)) {
            LOG_ERROR(DEC, "Invalid SEI data!");
            return -1;
        }
        sei_params->payloadData[i] = bs_read_u(bs, 8);
    }

    sei_params->payloadData[payload_size] = '\0';

    return 0;
}

int h264_read_sei_payload(struct va_dec_channel *chn, bit_stream *bs, uint32_t payload_type,
                          uint32_t payload_size, uint64_t pts)
{
    // get vmppSEIParameters
    int ret = -1;
    vmppSEI *sei_params = get_idle_sei_parameter(chn, pts);
    if (!sei_params) {
        LOG_ERROR(DEC, "No idle sei parameter!");
        return -1;
    }

    switch (payload_type) {
    case SEI_USER_DATA_UNREGISTERED:
        sei_params->payloadType = payload_type;
        sei_params->nalType = vmpp_SEI_PREFIX;
        ret = h264_read_user_data_unregistered(chn, bs, payload_size, sei_params);
        if (ret != 0) {
            set_sei_parameter_idle_data(chn, sei_params);
            return ret;
        }
        break;

    default:
        break;
    }
    h264_read_sei_end_bits(bs);

    return ret;
}

int h264_decode_sei(struct va_dec_channel *chn, bit_stream *bs, uint64_t pts)
{
    int res = -1;
    uint32_t payload_type = 0;
    uint32_t payload_size = 0;
    do {
        payload_type = h264_read_ff_coded_number(bs);
        payload_size = h264_read_ff_coded_number(bs);
        res = h264_read_sei_payload(chn, bs, payload_type, payload_size, pts);
        if (!res)
            break;
    } while (bs_more_data(bs));
    read_rbsp_trailing_bits(bs);
    return 0;
}

// return 0 success or -1 failed
int h264_decode_nal_unit(struct va_dec_channel *chn, uint8_t *buf, int size, uint64_t pts)
{
    int res = -1;
    bit_stream *bs = bs_new(chn, buf, size);
    if (!bs) {
        return -1;
    }

    nal_unit nal = {0};
    nal.forbidden_zero_bit = bs_read_u(bs, 1);
    nal.nal_ref_idc = bs_read_u(bs, 2);
    nal.nal_unit_type = bs_read_u(bs, 5);
    bs_free(chn, bs);
    bs = NULL;

    if (NAL_SEI != nal.nal_unit_type) {
        return -1;
    }

    struct va_sei_buf *sei_rbsp_buf = get_sei_idle_buffer(chn, size);
    if (!sei_rbsp_buf) {
        return -1;
    }

    uint8_t *rbsp_buf = (uint8_t *)sei_rbsp_buf->data;
    int rbsp_size = 0;
    // int read_size = 0;
    int i, j;

    i = 1;
    j = 0;
    while (i < size) {
        // next 24 bits: 0x000003
        if (i + 2 < size && buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x03) {
            rbsp_buf[j] = buf[i];
            rbsp_buf[j + 1] = buf[i + 1];
            i += 3;
            j += 2;
        } else if (i + 2 < size && buf[i] == 0x00 && buf[i + 1] == 0x00 &&
                   buf[i + 2] == 0x01) // next 24 bits 0x000001 start of next nal,
        {
            break;
        } else {
            rbsp_buf[j] = buf[i];
            i += 1;
            j += 1;
        }
    }
    // read_size = i;
    rbsp_size = j;

    bs = bs_new(chn, rbsp_buf, rbsp_size);
    if (!bs) {
        set_sei_idle_buffer(chn, rbsp_buf);
        rbsp_buf = NULL;
        return -1;
    }

    switch (nal.nal_unit_type) {
    case NAL_SEI:
        res = h264_decode_sei(chn, bs, pts);
        break;
    default:
        break;
    }

    if (bs_overrun(bs)) {
        res = -1;
    }

    set_sei_idle_buffer(chn, rbsp_buf);
    rbsp_buf = NULL;
    bs_free(chn, bs);
    bs = NULL;
    return res;
}

int h264_sei_decoder(struct va_dec_channel *chn, vmppStream *stream, uint64_t pts)
{
    int nal_start, nal_end;
    uint8_t *data = (uint8_t *)stream->stream;
    int len = stream->len;
    int res = -1;
    int sei_decode = -1;
    while (find_nal_unit(data, len, &nal_start, &nal_end) > 0) {
        data += nal_start;
        res = h264_decode_nal_unit(chn, data, nal_end - nal_start, pts);
        if (0 == res) {
            sei_decode = 0;
        }
        data += (nal_end - nal_start);
        len -= nal_end;
    }
    return sei_decode;
}
//------------------------h264 sei end---------------------//

int hevc_nal_to_rbsp(const int nal_header_size, const uint8_t *nal_buf, int *nal_size,
                     uint8_t *rbsp_buf, int *rbsp_size)
{
    int i;
    int j = 0;
    int count = 0;

    for (i = nal_header_size; i < *nal_size; i++) {
        // 0x000000, 0x000001 or 0x000002 naver occur
        if ((count == 2) && (nal_buf[i] < 0x03)) {
            return -1;
        }

        if ((count == 2) && (nal_buf[i] == 0x03)) {
            // check the 4th byte after 0x000003, except when cabac_zero_word is used,
            // in which case the last three bytes of this NAL unit must be 0x000003
            if ((i < *nal_size - 1) && (nal_buf[i + 1] > 0x03)) {
                return -1;
            }

            // if cabac_zero_word is used, the final byte of this NAL unit(0x03) is discarded,
            // and the last two bytes of RBSP must be 0x0000
            if (i == *nal_size - 1) {
                break;
            }

            i++;
            count = 0;
        }

        if (j >= *rbsp_size) {
            // error, out of range
            return -1;
        }

        rbsp_buf[j] = nal_buf[i];
        if (nal_buf[i] == 0x00) {
            count++;
        } else {
            count = 0;
        }
        j++;
    }

    *nal_size = i;
    *rbsp_size = j;
    return j;
}

int hevc_more_rbsp_data(bit_stream *bs)
{
    if (bs_eof(bs)) {
        return 0;
    }
    if (bs_peek_u1(bs) == 1) {
        return 0;
    } // if next bit is 1, reached the stop bit
    return 1;
}

int hevc_read_to_ff_number(bit_stream *bs)
{
    int n1 = 0;
    int n2;
    do {
        n2 = bs_read_u8(bs);
        n1 += n2;
    } while (n2 == 0xff);

    return n1;
}

int hevc_read_user_data_unregistered(struct va_dec_channel *chn, bit_stream *bs, int payloadSize,
                                      vmppSEI *sei_params)
{
    int i;
    struct va_sei_buf *sei_buf = NULL;
    sei_buf = get_sei_idle_buffer(chn, payloadSize + 1);
    if (!sei_buf) {
        LOG_ERROR(DEC, "No idle sei buffer!");
        return -1;
    }

    sei_params->payloadDataSize = payloadSize;
    sei_params->payloadData = sei_buf->data;

    for (i = 0; i < 16; i++) {
        if (bs_eof(bs)) {
            LOG_ERROR(DEC, "Invalid SEI data!");
            return -1;
        }
        sei_params->payloadData[i] = bs_read_u(bs, 8);

    }
    for (i = 16; i < payloadSize; i++) {
        if (bs_eof(bs)) {
            LOG_ERROR(DEC, "Invalid SEI data!");
            return -1;
        }
        sei_params->payloadData[i] = bs_read_u(bs, 8);
    }
    sei_params->payloadData[payloadSize] = '\0';
    return 0;
}

void hevc_read_rbsp_trailing_bits(bit_stream *bs)
{
    // int rbsp_stop_one_bit = bs_read_u1( bs ); // equal to 1
    bs_read_u1(bs); // // only read and move forward

    while (!bs_byte_aligned(bs)) {
        // int rbsp_alignment_zero_bit = bs_read_u1(bs); // equal to 0 7 bits
        bs_read_u1(bs); //// only read and move forward
    }
}

void hevc_read_sei_end_bits(bit_stream *bs)
{
    // if the message doesn't end at a byte border
    if (!bs_byte_aligned(bs)) {
        if (!bs_read_u1(bs)) {
            // equel to 0, illegal
        }
        while (!bs_byte_aligned(bs)) {
            if (bs_read_u1(bs)) {
                // hevc_read_user_data_unregistered
            }
        }
    }
    hevc_read_rbsp_trailing_bits(bs);
}

int hevc_read_sei_payload(struct va_dec_channel *chn, bit_stream *bs, int payloadType,
                           int payloadSize, uint64_t input_pts, uint32_t nal_type)
{
    vmppSEI *sei_params = get_idle_sei_parameter(chn, input_pts);
    int res = -1;
    if (!sei_params) {
        LOG_ERROR(DEC, "No idle sei parameter!");
        return -1;
    }
    sei_params->nalType = nal_type;
    if (SEI_USER_DATA_UNREGISTERED == payloadType) {
        sei_params->payloadType = payloadType;
        res = hevc_read_user_data_unregistered(chn, bs, payloadSize, sei_params);
        if (res != 0) {
            set_sei_parameter_idle_data(chn, sei_params);
            return res;
        }
    }
    hevc_read_sei_end_bits(bs);
    return res;
}

int hevc_decode_sei(struct va_dec_channel *chn, bit_stream *bs, uint64_t input_pts, int nal_type)
{
    int res = -1;
    uint32_t payload_size = 0;
    uint32_t payload_type = 0;
    if (!chn || !bs) {
        return -1;
    }

    do {
        payload_type = hevc_read_to_ff_number(bs);
        payload_size = hevc_read_to_ff_number(bs);
        res = hevc_read_sei_payload(chn, bs, payload_type, payload_size, input_pts, nal_type);
        if (!res)
            break;
        hevc_read_rbsp_trailing_bits(bs);
    } while (hevc_more_rbsp_data(bs));

    return res;
}

int hevc_decode_nal_unit(struct va_dec_channel *chn, uint8_t *buf, int size, uint64_t input_pts)
{
    int sei_docode = -1;

    bit_stream *bs = bs_new(chn, buf, size);
    if (!bs) {
        return -1;
    }

    hevc_nal_unit nal = {0};
    nal.forbidden_zero_bit = bs_read_u(bs, 1);
    nal.nal_unit_type = bs_read_u(bs, 6);
    nal.nuh_reserved_zero_6bits = bs_read_u(bs, 6);
    nal.nuh_temporal_id_plus1 = bs_read_u(bs, 3);
    bs_free(chn, bs);
    bs = NULL;

    if (nal.nal_unit_type != vmpp_SEI_PREFIX && nal.nal_unit_type != vmpp_SEI_SUFFIX) {
        return -1;
    }

    int nal_size = size;
    int rbsp_size = size;
    struct va_sei_buf *sei_rbsp_buf = get_sei_idle_buffer(chn, rbsp_size);
    if (!sei_rbsp_buf) {
        return -1;
    }

    uint8_t *rbsp_buf = (uint8_t *)sei_rbsp_buf->data;
    if (!rbsp_buf) {
        return -1;
    }

    int rc = hevc_nal_to_rbsp(2, buf, &nal_size, rbsp_buf, &rbsp_size);
    if (rc < 0) {
        // free(rbsp_buf);
        set_sei_idle_buffer(chn, rbsp_buf);
        return -1;
    }
    bs = bs_new(chn, rbsp_buf, rbsp_size);
    if (!bs) {
        set_sei_idle_buffer(chn, rbsp_buf);
        return -1;
    }

    switch (nal.nal_unit_type) {
    case vmpp_SEI_PREFIX:
    case vmpp_SEI_SUFFIX:
        sei_docode = hevc_decode_sei(chn, bs, input_pts, nal.nal_unit_type);
        break;
    default:
        break;
    }
    set_sei_idle_buffer(chn, rbsp_buf);
    bs_free(chn, bs);
    return sei_docode;
}

vmppResult hevc_sei_decoder(struct va_dec_channel *chn, vmppStream *stream, uint64_t input_pts)
{
    int nal_start, nal_end;
    uint8_t *data = (uint8_t *)stream->stream;
    int len = stream->len;
    int sei_decode = -1;
    int res = vmpp_RSLT_ERR_INVALID_DATA;
    while (find_nal_unit(data, len, &nal_start, &nal_end) > 0) {
        data += nal_start;
        sei_decode = hevc_decode_nal_unit(chn, data, nal_end - nal_start, input_pts);
        if (0 == sei_decode) {
            res = vmpp_RSLT_OK;
        }
        data += (nal_end - nal_start);
        len -= nal_end;
    }
    return res;
}

vmppResult sei_decoder(struct va_dec_channel *chn, vmppStream *stream, uint64_t pts)
{
    vmppResult ret = vmpp_RSLT_OK;
    int sei_ret = -1;
    if (!chn || !stream) {
        return vmpp_RSLT_ERR_INVALID_PARAMS;
    }

    struct va_dec_channel *inst = (struct va_dec_channel *)chn;
    if (vmpp_CODEC_DEC_H264 == inst->params.codecType) {
        sei_ret = h264_sei_decoder(chn, stream, pts);
    } else if (vmpp_CODEC_DEC_HEVC == inst->params.codecType) {
        sei_ret = hevc_sei_decoder(chn, stream, pts);
    } else {
        ret = vmpp_RSLT_ERR_INVALID_PARAMS;
    }
    if (sei_ret != 0) {
        ret = vmpp_RSLT_ERR_INVALID_DATA;
    }
    return ret;
}
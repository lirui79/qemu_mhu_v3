#ifndef __TS_PROTOCOL_H__
#define __TS_PROTOCOL_H__

#include <stdint.h>

//----------------------------------------------------------------
//    common definition
//----------------------------------------------------------------

#define TS_PACKET_LEN(st)               (sizeof(st) / sizeof(uint32_t))
#define TS_PACKET_LEN_MAX               128 //dwords

/*
 * All structures use plain uint32_t fields to avoid bitfield alignment
 * and layout differences between AArch64 (Host/A76) and ARMv7-R (TS/R52).
 *
 * Command/data header dword layout:
 *   [7:0]   code    (command or data code)
 *   [15:8]  flag    (reserved flags)
 *   [31:16] length  (packet length in dwords)
 */
#define TS_HDR_CODE(h)     ((h) & 0xFFu)
#define TS_HDR_FLAG(h)     (((h) >> 8) & 0xFFu)
#define TS_HDR_LENGTH(h)   (((h) >> 16) & 0xFFFFu)
#define TS_HDR_MAKE(c,f,l) (((uint32_t)(c) & 0xFFu) | \
                            (((uint32_t)(f) & 0xFFu) << 8) | \
                            (((uint32_t)(l) & 0xFFFFu) << 16))

//----------------------------------------------------------------
//    command definition: HOST -> GPU
//----------------------------------------------------------------

/* command code */
#define TS_CMD_CODE_QUERY               0x01
#define TS_CMD_CODE_CREATE_PROCESS      0x02
#define TS_CMD_CODE_DESTROY_PROCESS     0x03
#define TS_CMD_CODE_CREATE_QUEUE        0x04
#define TS_CMD_CODE_DESTROY_QUEUE       0x05

/* query type */
#define TS_QUERY_TYPE_HW                0x01
#define TS_QUERY_TYPE_SW                0x02
#define TS_QUERY_TYPE_TS                0x03
#define TS_QUERY_TYPE_PROCESS           0x04
#define TS_QUERY_TYPE_QUEUE             0x05

/* queue type */
#define TS_QUEUE_TYPE_COMPUTE           0x00
#define TS_QUEUE_TYPE_COMPUTE_AQL       0x01
#define TS_QUEUE_TYPE_SDMA              0x02

/*
 * dword layout for all commands:
 *   [0] header (code:8 flag:8 length:16)
 *   [1+] command-specific payload
 */

/* query command: [0] header, [1] type:8 | param:24 */
typedef struct {
    uint32_t        header;
    uint32_t        type_param;
} ts_cmd_query;

/* create process command: [0] header, [1] pasid:16 | vmid:16 */
typedef struct {
    uint32_t        header;
    uint32_t        pasid_vmid;
} ts_cmd_create_process;

/* destroy process command: [0] header, [1] pasid:16 | reserved:16 */
typedef struct {
    uint32_t        header;
    uint32_t        pasid_rsvd;
} ts_cmd_destroy_process;

/* create queue command
 * Field order arranged so that 64-bit pairs (ringbuf_base, rptr, wptr)
 * land on 8-byte-aligned offsets, avoiding misaligned 64-bit stores
 * when MMU is off (Device memory prohibits unaligned access).
 *
 * [0] header (code:8 flag:8 length:16)
 * [1] queue_id:16 | pasid:16
 * [2] type:8 | priority:8 | doorbell_idx:16
 * [3] ringbuf_size
 * [4] ringbuf_base_lo
 * [5] ringbuf_base_hi
 * [6] rptr_lo
 * [7] rptr_hi
 * [8] wptr_lo
 * [9] wptr_hi
 */
typedef struct {
    uint32_t        header;              /* [0] */
    uint32_t        queue_pasid;         /* [1] queue_id:16 | pasid:16 */
    uint32_t        type_pri_db;         /* [2] type:8 | priority:8 | doorbell_idx:16 */
    uint32_t        ringbuf_size;        /* [3] (moved before ringbuf_base for 8B-alignment) */
    uint32_t        ringbuf_base_lo;     /* [4] */
    uint32_t        ringbuf_base_hi;     /* [5] */
    uint32_t        rptr_lo;             /* [6] */
    uint32_t        rptr_hi;             /* [7] */
    uint32_t        wptr_lo;             /* [8] */
    uint32_t        wptr_hi;             /* [9] */
} ts_cmd_create_queue;

/* destroy queue command
 * [0] header (code:8 flag:8 length:16)
 * [1] queue_id:16 | pasid:16
 */
typedef struct {
    uint32_t        header;
    uint32_t        queue_pasid;        /* [15:0] queue_id, [31:16] pasid */
} ts_cmd_destroy_queue;

//----------------------------------------------------------------
//    data definition: GPU -> HOST
//----------------------------------------------------------------

/* data code */
#define TS_DATA_CODE_HW_INFO            0x01
#define TS_DATA_CODE_SW_INFO            0x02
#define TS_DATA_CODE_TS_STAT            0x03

/* hw info: [0] header, [1] vendor:16|dev:16, [2] sm:8|core:8|warp:8|wpc:8,
 *          [3] smem_size, [4] l2, [5] l1d, [6] l1i */
typedef struct {
    uint32_t        header;
    uint32_t        vendor_device;       /* [15:0] vendor_id, [31:16] device_id */
    uint32_t        sm_config;           /* [7:0] sm_count, [15:8] core_per_sm, [23:16] warp_size, [31:24] warp_per_core */
    uint32_t        smem_size;
    uint32_t        l2_cache_size;
    uint32_t        l1_dcache_size;
    uint32_t        l1_icache_size;
} ts_data_hw_info;

/* sw info: [0] header, [1] ts_fw_version */
typedef struct {
    uint32_t        header;
    uint32_t        ts_fw_version;
} ts_data_sw_info;

#endif

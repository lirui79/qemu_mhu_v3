/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                           *.c vcodec source code                             **
*********************************************************************************/

#include "vcodec.h"
#include "cmdr52_mgr.h"
#include "cmdr52_proc.h"
#include "vcx_cmdbuf_obj.h"

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif

#ifdef __cplusplus
extern "C" {
#endif

int              vce_vcmd_init(vcmd_mgr_t **_vcmd_mgr);

int              vcd_vcmd_init(vcmd_mgr_t **_vcmd_mgr);

void             vce_vcmd_exit(vcmd_mgr_t *vcmd_mgr);

void             vcd_vcmd_exit(vcmd_mgr_t *vcmd_mgr);

#ifdef __cplusplus
}
#endif

static vcodec_config_t g_vcodec_config = {
    .vcmd_isr_polling = 1,
    .vsi_kloglvl = LOGLVL_ERROR,
    .arbiter_weight = 0x1d,
    .arbiter_urgent = 0x00,
    .arbiter_timewindow = 0x1d,
    .arbiter_bw_overflow = 0x00,
    .sw_timeout_time = 1000 * 1000, //SW_TIMEOUT_TIME_FOR_ARBITER,
    .ddr_offset = 0,
#ifdef SUPPORT_MMU
    .mmu_enable = 0,
#endif
};




vcodec_config_t *vcodec_get_config(void) {
    return &g_vcodec_config;
}

int32_t          vcodecr52_init() {
    uint32_t r52coreID = 0;
    cmdr52_mgr_init(cmdr52_mgr_get(), r52coreID);
    return vcx_vcmd_init(NULL);
}

int32_t          vcodec_init_vcmd(cmdMsg_t *cmdMsg) {
    return 0;
}

int32_t          vcx_vcmd_init(cmdMsg_t *cmdMsg) {
    vce_vcmd_init(&cmdr52_mgr_get()->mtb[VCMD_MGR_ID_ENC]);
    vcd_vcmd_init(&cmdr52_mgr_get()->mtb[VCMD_MGR_ID_DEC]);
//ts_printf("%s:%s:%d started\n", __FILE__, __func__, __LINE__);
    return cmdr52_start_mgr();
}

int32_t          vcx_vcmd_exit(cmdMsg_t *cmdMsg) {
    vce_vcmd_exit(cmdr52_mgr_get()->mtb[VCMD_MGR_ID_ENC]);
    vcd_vcmd_exit(cmdr52_mgr_get()->mtb[VCMD_MGR_ID_DEC]);
    return 0;
}

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
**                            include vcodec header                             **
*********************************************************************************/

#ifndef _VCODEC_H_
#define _VCODEC_H_

#include "cmdef.h"

enum {
	LOGLVL_VERBOSE = 0,		// log all
	LOGLVL_FLOW,			// log all debug msg
	LOGLVL_CONFIG,			// log ctrl/config info, mostly at beginning
	LOGLVL_BRIEF,			// log critical point
	LOGLVL_WARNING,			// log warning msg
	LOGLVL_ERROR,			// log error
};
/* declarations */

#define vcmd_klog(lvl, fmt, ...) {\
	if (lvl == LOGLVL_ERROR)	\
		ts_printf("ERROR hantrovcmd: " fmt, ##__VA_ARGS__);	\
	else if (lvl >= vcodec_get_config()->vsi_kloglvl)		\
		ts_printf("INFO hantrovcmd: " fmt, ##__VA_ARGS__);	\
}


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
//    uint32_t vcmd_supported;
    uint32_t     vcmd_isr_polling;
    uint32_t     vsi_kloglvl;
    uint32_t     arbiter_weight;
    uint32_t     arbiter_urgent;
    uint32_t     arbiter_timewindow;
    uint32_t     arbiter_bw_overflow;
    uint64_t     sw_timeout_time;
    uint64_t     ddr_offset;
#ifdef SUPPORT_MMU
    uint32_t     mmu_enable;
#endif
} vcodec_config_t;


vcodec_config_t *vcodec_get_config(void);

int32_t          vcodecr52_init(void);

void             vcodecr52_exit(void);

int32_t          vcodec_init_vcmd(cmdMsg_t *cmdMsg);

int32_t          vcx_vcmd_init(cmdMsg_t *cmdMsg);

int32_t          vcx_vcmd_exit(cmdMsg_t *cmdMsg);

#ifdef __cplusplus
}
#endif

#endif /*_VCODEC_H_*/

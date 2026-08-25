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
**                        include vcx watch dog header                          **
*********************************************************************************/

#ifndef _VCX_WATCH_DOG_H_
#define _VCX_WATCH_DOG_H_

#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef SUPPORT_WATCHDOG

void hook_vcmd_watchdog(struct hantrovcmd_dev *dev, int succeed);

void watchdog_reset_vcmd_arbiter(struct hantrovcmd_dev *dev);

int watchdog_wait_vcmd_aborted(struct hantrovcmd_dev *dev);

int watchdog_stop_vcmd(struct hantrovcmd_dev *dev);

void _vcmd_watchdog_process(struct hantrovcmd_dev *dev);

u32 dev_wait_job_num(struct hantrovcmd_dev *dev);


void _vcmd_watchdog_create(struct hantrovcmd_dev *dev, unsigned int timeout);

void _vcmd_watchdog_start(struct hantrovcmd_dev *dev, int irq);

void _vcmd_watchdog_stop(struct hantrovcmd_dev *dev, int irq);

void _vcmd_watchdog_feed(struct hantrovcmd_dev *dev, int irq);

void _vcmd_watchdog_delete(struct hantrovcmd_dev *dev);

#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCX_WATCH_DOG_H_*/

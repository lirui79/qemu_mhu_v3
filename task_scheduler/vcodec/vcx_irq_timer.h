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
**                         include vcx irq timer header                         **
*********************************************************************************/

#ifndef _VCX_IRQ_TIMER_H_
#define _VCX_IRQ_TIMER_H_

#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIMEOUT_IRQ_TIMER

#ifdef  TIMEOUT_IRQ_TIMER

void _vcmd_timeout_create_timer(struct hantrovcmd_dev *dev, unsigned int timeout);

void _vcmd_timeout_delete_timer(struct hantrovcmd_dev *dev);

void _vcmd_timeout_start_timer(struct hantrovcmd_dev *dev);

void _vcmd_timeout_stop_timer(struct hantrovcmd_dev *dev);


#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCX_IRQ_TIMER_H_*/

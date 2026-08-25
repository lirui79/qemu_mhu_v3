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
**                         include vcx vcmd irq header                          **
*********************************************************************************/

#ifndef _VCX_VCMD_IRQ_H_
#define _VCX_VCMD_IRQ_H_

#include "vcx_vcmd_priv.h"

enum irqreturn {
    IRQ_NONE        = (0 << 0),
    IRQ_HANDLED     = (1 << 0),
    IRQ_WAKE_THREAD = (1 << 1),
};

typedef enum irqreturn irqreturn_t;

#ifdef __cplusplus
extern "C" {
#endif



irqreturn_t hantrovcmd_isr(int irq, void *handler);




#ifdef __cplusplus
}
#endif

#endif /* _VCX_VCMD_IRQ_H_ */
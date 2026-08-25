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
**                       include vcx irq simulation header                      **
*********************************************************************************/

#ifndef _VCX_IRQ_SIMULATION_H_
#define _VCX_IRQ_SIMULATION_H_

#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef IRQ_SIMULATION

void _irq_simul_add_timer(struct cmdbuf_obj *obj);

void _irq_simul_init(void *vcmd_mgr);

#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCX_IRQ_SIMULATION_H_*/

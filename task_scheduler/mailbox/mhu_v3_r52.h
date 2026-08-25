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
**                          include mhu v3 r52 header                           **
*********************************************************************************/

#ifndef _MHU_V3_R52_H_
#define _MHU_V3_R52_H_

#include "inc.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t mhu_v3_recv_data(uint32_t r52id, uint8_t *cmdMsg, uint32_t *cmdSize);

int32_t mhu_v3_send_data(uint32_t r52id, const uint8_t *cmdMsg, uint32_t cmdSize);


#ifdef __cplusplus
}
#endif

#endif /*_MHU_V3_R52_H_*/

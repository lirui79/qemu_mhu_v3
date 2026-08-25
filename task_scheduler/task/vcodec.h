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
**                               include vcodec header                          **
*********************************************************************************/

#ifndef _VCODEC_INC_H_
#define _VCODEC_INC_H_

#ifdef __FREERTOS__
#include "osal_freertos.h" /* needed for the _IOW etc stuff used later */
#endif



#ifdef __cplusplus
extern "C" {
#endif


//R52 core
int32_t              vcodecr52_init(void);

void                 vcodecr52_exit(void);



#ifdef __cplusplus
}
#endif


#endif //_VCODEC_INC_H_
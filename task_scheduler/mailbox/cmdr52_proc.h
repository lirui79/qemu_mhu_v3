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
**                        include cmd r52 proc header                           **
*********************************************************************************/

#ifndef _COMMANDR52_PROC_H_
#define _COMMANDR52_PROC_H_

#include "cmdef.h"

#ifdef __cplusplus
extern "C" {
#endif

void        cmdr52_set_callback(void);

void        cmdr52_proc_loop(void);

#ifdef __cplusplus
}
#endif

#endif /*_COMMANDR52_PROC_H_*/

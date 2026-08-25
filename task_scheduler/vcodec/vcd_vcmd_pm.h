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
**                         include vcd vcmd pm header                           **
*********************************************************************************/

#ifndef _VCD_VCMD_PM_H_
#define _VCD_VCMD_PM_H_

#define CONFIG_DEC_PM


#ifdef __cplusplus
extern "C" {
#endif


#ifdef CONFIG_DEC_PM


int vcmd_vcd_pm_suspend(void *handler);

int vcmd_vcd_pm_resume(void *handler);


#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCD_VCMD_PM_H_*/
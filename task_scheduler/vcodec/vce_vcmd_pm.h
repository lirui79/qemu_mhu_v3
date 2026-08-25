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
**                         include vce vcmd pm header                           **
*********************************************************************************/

#ifndef _VCE_VCMD_PM_H_
#define _VCE_VCMD_PM_H_

#define CONFIG_ENC_PM

#ifdef __cplusplus
extern "C" {
#endif


#ifdef CONFIG_ENC_PM


int vcmd_vce_pm_suspend(void *handler);

int vcmd_vce_pm_resume(void *handler);


#endif

#ifdef __cplusplus
}
#endif

#endif /*_VCE_VCMD_PM_H_*/
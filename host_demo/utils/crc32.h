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
**                             include crc32 header                             **
*********************************************************************************/

#ifndef _FREERTOS_CRC32_H_
#define _FREERTOS_CRC32_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32Initial(void);

uint32_t crc32Update(const uint8_t *buffer, size_t bufferLength, uint32_t crc);

uint32_t crc32Final(uint32_t crc);

uint32_t crc32_calc(const uint8_t *buffer, size_t bufferLength);

#ifdef __cplusplus
}
#endif

#endif /*_FREERTOS_CRC32_H_*/

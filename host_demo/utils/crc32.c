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
**                            *.c crc32 source code                             **
*********************************************************************************/
#include <stdint.h>
#include <stddef.h>

#include "crc32.h"

#define CRC32_INITIAL      0xFFFFFFFFUL
#define CRC32_FINAL_XOR    0xFFFFFFFFUL
#define CRC32_POLY         0xEDB88320UL // 标准多项式的位反转形式


uint32_t crc32Initial(void) {
    return CRC32_INITIAL;
}

// 1. 初始化/更新 CRC 状态（允许分片计算，适合流式数据）
uint32_t crc32Update(const uint8_t *buffer, size_t bufferLength, uint32_t crc) {
    for (size_t i = 0; i < bufferLength; i++) {
        crc ^= buffer[i];
        // 8次位操作，实际工程中可手动展开循环以提升性能
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ CRC32_POLY : (crc >> 1);
        }
    }
    return crc;
}

// 2. 获取最终 CRC32 值
uint32_t crc32Final(uint32_t crc) {
    return crc ^ CRC32_FINAL_XOR;
}

uint32_t crc32_calc(const uint8_t *buffer, size_t bufferLength) {
    uint32_t crc = crc32Initial();//CRC32_INITIAL;
    crc = crc32Update(buffer, bufferLength, crc);
    return crc32Final(crc);
}
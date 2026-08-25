#ifndef __UART_PL011_H__
#define __UART_PL011_H__

#include <stdint.h>

typedef volatile struct __attribute__((packed)) {
    uint32_t DR;               /* 0x0 Data Register */
    uint32_t RSRECR;           /* 0x4 Receive status / error clear register */
    uint32_t _reserved0[4];    /* 0x8 - 0x14 reserved */
    const uint32_t FR;         /* 0x18 Flag register */
    uint32_t _reserved1;       /* 0x1C reserved */
    uint32_t ILPR;             /* 0x20 Low-power counter register */
    uint32_t IBRD;             /* 0x24 Integer baudrate register */
    uint32_t FBRD;             /* 0x28 Fractional baudrate register */
    uint32_t LCRH;             /* 0x2C Line control register */
    uint32_t CR;               /* 0x30 Control register */
} uart_registers;

#define DR_DATA_MASK        (0xFFu)

#define FR_BUSY             (1 << 3u)
#define FR_RXFE             (1 << 4u)
#define FR_TXFF             (1 << 5u)

#define RSRECR_ERR_MASK     (0xFu)

#define LCRH_FEN            (1 << 4u)
#define LCRH_PEN            (1 << 1u)
#define LCRH_EPS            (1 << 2u)
#define LCRH_STP2           (1 << 3u)
#define LCRH_SPS            (1 << 7u)
#define CR_UARTEN           (1 << 0u)

#define LCRH_WLEN_5BITS     (0u << 5u)
#define LCRH_WLEN_6BITS     (1u << 5u)
#define LCRH_WLEN_7BITS     (2u << 5u)
#define LCRH_WLEN_8BITS     (3u << 5u)

#endif

#ifndef __UART_PL011_H__
#define __UART_PL011_H__

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Register map                                                       */
/* ------------------------------------------------------------------ */

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
} uart_registers_t;

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

/* ------------------------------------------------------------------ */
/* Config values                                                      */
/* ------------------------------------------------------------------ */

/* uart configuration */
typedef struct {
    uint8_t     data_bits;      /* data word length: 5, 6, 7 or 8 */
    uint8_t     stop_bits;      /* stop bits: 1 or 2 */
    uint8_t     parity;         /* 0 = none, non-zero = even parity */
    uint32_t    baudrate;       /* baudrate, range [110, 460800] */
} uart_config_t;

/* ------------------------------------------------------------------ */
/* API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief  Configure a UART controller, applied to the UART after disabling it
 *         and flushing the FIFO; call with all fields of config filled in
 * @param  uart_base: base address of the UART controller
 * @param  config: UART configuration (data bits, stop bits, parity, baudrate)
 * @return TS_OK for success, TS_ERR_INVALID_ARG for out-of-range values
 */
int uart_configure(uint32_t uart_base, uart_config_t* config);

/**
 * @brief  Transmit a single character, blocks until the transmit FIFO has room
 * @param  uart_base: base address of the UART controller
 * @param  c: character to transmit
 * @return None
 */
void uart_putchar(uint32_t uart_base, char c);

/**
 * @brief  Transmit a null-terminated string
 * @param  uart_base: base address of the UART controller
 * @param  data: pointer to the string to transmit
 * @return None
 */
void uart_write(uint32_t uart_base, const char* data);

/**
 * @brief  Receive a character without blocking
 * @param  uart_base: base address of the UART controller
 * @param  c: pointer to memory to save the received character
 * @return TS_OK for success, TS_ERR_EMPTY if the receive FIFO is empty,
 *         TS_ERR_IO if the character was received with an error
 */
int uart_getchar(uint32_t uart_base, char* c);

#endif

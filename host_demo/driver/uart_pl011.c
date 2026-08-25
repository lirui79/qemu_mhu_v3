#include "system.h"
#include "uart_pl011.h"

static const uint32_t refclock = 50000000u; /* 50 MHz */

int uart_configure(uint32_t uart_base, uart_config* config)
{
    uart_registers* uart = (uart_registers*)(uint64_t)uart_base;

    /* Validate config */
    if (config->data_bits < 5u || config->data_bits > 8u) {
        return -1;
    }
    if (config->stop_bits == 0u || config->stop_bits > 2u) {
        return -2;
    }
    if (config->baudrate < 110u || config->baudrate > 460800u) {
        return -3;
    }

    /* Disable the UART */
    uart->CR &= ~CR_UARTEN;

    /* Finish any current transmission, and flush the FIFO */
    while (uart->FR & FR_BUSY);
    uart->LCRH &= ~LCRH_FEN;

    /* Set baudrate */
    uint32_t baudrate_divisor = refclock / (16u * config->baudrate);
    uart->IBRD = (uint16_t)baudrate_divisor;

    uint32_t lcrh = 0u;

    /* Set data word size */
    switch (config->data_bits)
    {
    case 5:
        lcrh |= LCRH_WLEN_5BITS;
        break;
    case 6:
        lcrh |= LCRH_WLEN_6BITS;
        break;
    case 7:
        lcrh |= LCRH_WLEN_7BITS;
        break;
    case 8:
        lcrh |= LCRH_WLEN_8BITS;
        break;
    }

    /* Set parity. If enabled, use even parity */
    if (config->parity) {
        lcrh |= LCRH_PEN;
        lcrh |= LCRH_EPS;
        lcrh |= LCRH_SPS;
    } else {
        lcrh &= ~LCRH_PEN;
        lcrh &= ~LCRH_EPS;
        lcrh &= ~LCRH_SPS;
    }

    /* Set stop bits */
    if (config->stop_bits == 1u) {
        lcrh &= ~LCRH_STP2;
    } else if (config->stop_bits == 2u) {
        lcrh |= LCRH_STP2;
    }

    /* Enable FIFOs */
    lcrh |= LCRH_FEN;

    uart->LCRH = lcrh;

    /* Enable the UART */
    uart->CR |= CR_UARTEN;

    return 0;
}

void uart_putchar(uint32_t uart_base, char c)
{
    uart_registers* uart = (uart_registers*)(uint64_t)uart_base;
    while (uart->FR & FR_TXFF);
    uart->DR = c;
}

void uart_write(uint32_t uart_base, const char* data)
{
    while (*data) {
        uart_putchar(uart_base, *data++);
    }
}

int uart_getchar(uint32_t uart_base, char* c)
{
    uart_registers* uart = (uart_registers*)(uint64_t)uart_base;
    if (uart->FR & FR_RXFE) {
        return -1;
    }

    *c = uart->DR & DR_DATA_MASK;
    if (uart->RSRECR & RSRECR_ERR_MASK) {
        /* The character had an error */
        uart->RSRECR &= RSRECR_ERR_MASK;
        return -2;
    }
    return 0;
}


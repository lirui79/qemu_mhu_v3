#include <stdarg.h>
#include <stdint.h>
#include "system.h"

static void print_num(uint32_t uart_base, uint64_t num, int base)
{
    char buffer[33];
    char *p = buffer;
    const char *hex_chars = "0123456789ABCDEF";
    if (num == 0) {
        uart_putchar(uart_base, '0');
        return;
    }
    while (num > 0) {
        *p++ = hex_chars[num % base];
        num /= base;
    }
    while (p > buffer) {
        uart_putchar(uart_base, *--p);
    }
}

int ts_printf(const char *fmt, ...)
{
    uint32_t uart_base = (get_cpu_id() == 0) ? UART0_BASE : UART1_BASE;
    va_list args;
    uint32_t flags;
    va_start(args, fmt);

    /* 整行串行化输出：防止同核多任务/ISR 逐字符抢占导致交错乱码 */
    flags = arch_local_irq_save();

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            /* skip flags/width/length modifiers (e.g. %08x, %llx) */
            while (*fmt == '0' || *fmt == '-' || *fmt == ' ' || *fmt == '+' ||
                   *fmt == '#' || (*fmt >= '1' && *fmt <= '9'))
                fmt++;
            while (*fmt == 'l' || *fmt == 'h')
                fmt++;
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int val = va_arg(args, int);
                    if (val < 0) {
                        uart_putchar(uart_base, '-');
                        val = -val;
                    }
                    print_num(uart_base, (uint64_t)val, 10);
                    break;
                }
                case 'u':
                    print_num(uart_base, va_arg(args, unsigned int), 10);
                    break;
                case 'x':
                    print_num(uart_base, va_arg(args, unsigned int), 16);
                    break;
                case 'U':
                    print_num(uart_base, va_arg(args, uint64_t), 10);
                    break;
                case 'X':
                    print_num(uart_base, va_arg(args, uint64_t), 16);
                    break;
                case 's': {
                    const char *str = va_arg(args, const char *);
                    while (*str) {
                        uart_putchar(uart_base, *str++);
                    }
                    break;
                }
                case 'c':
                    uart_putchar(uart_base, (char)va_arg(args, int));
                    break;
                case '%':
                    uart_putchar(uart_base, '%');
                    break;
                default:
                    uart_putchar(uart_base, '%');
                    uart_putchar(uart_base, *fmt);
                    break;
            }
        } else {
            uart_putchar(uart_base, *fmt);
        }
        fmt++;
    }

    arch_local_irq_restore(flags);

    va_end(args);
    return 0;
}

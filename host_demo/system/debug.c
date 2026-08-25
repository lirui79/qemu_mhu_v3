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


/*------------------------------------------------------------------------------
 * uart_put_hex32 - Print 32-bit hex value (8 digits)
 *----------------------------------------------------------------------------*/
void uart_put_hex32(uint32_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--)
        uart_putchar(UART0_BASE, hex[(val >> (i * 4)) & 0xF]);
}

/*------------------------------------------------------------------------------
 * uart_puts - Output a null-terminated string
 *----------------------------------------------------------------------------*/
void uart_puts(const char *s)
{
    uint64_t flags = arch_local_irq_save();
    while (*s) {
        uart_putchar(UART0_BASE, *s++);
    }
    arch_local_irq_restore(flags);
}


int ts_printf(const char *fmt, ...)
{
    uint32_t uart_base = UART0_BASE;
    va_list args;
    uint64_t flags;
    va_start(args, fmt);

    /* 整行串行化输出：防止同核多任务/ISR 逐字符抢占导致交错乱码 */
    flags = arch_local_irq_save();

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            int is_long = 0;
            int is_longlong = 0;
            /* skip flags/width */
            while (*fmt == '0' || *fmt == '-' || *fmt == ' ' || *fmt == '+' ||
                   *fmt == '#' || (*fmt >= '1' && *fmt <= '9'))
                fmt++;
            /* track length modifiers: %l → long, %ll → long long */
            while (*fmt == 'l' || *fmt == 'h') {
                if (*fmt == 'l') {
                    if (is_long) is_longlong = 1;
                    else is_long = 1;
                }
                fmt++;
            }
            switch (*fmt) {
                case 'd':
                case 'i': {
                    long long val = is_longlong ? va_arg(args, long long) :
                                    (is_long ? va_arg(args, long) : va_arg(args, int));
                    if (val < 0) {
                        uart_putchar(uart_base, '-');
                        val = -val;
                    }
                    print_num(uart_base, (uint64_t)val, 10);
                    break;
                }
                case 'u': {
                    unsigned long long val = is_longlong ? va_arg(args, unsigned long long) :
                                             (is_long ? va_arg(args, unsigned long) :
                                                        va_arg(args, unsigned int));
                    print_num(uart_base, val, 10);
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned long long val = is_longlong ? va_arg(args, unsigned long long) :
                                             (is_long ? va_arg(args, unsigned long) :
                                                        va_arg(args, unsigned int));
                    print_num(uart_base, val, 16);
                    break;
                }
                case 'p': {
                    void *val = va_arg(args, void *);
                    print_num(uart_base, (uint64_t)(uintptr_t)val, 16);
                    break;
                }
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

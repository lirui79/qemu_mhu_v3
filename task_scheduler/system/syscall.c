/*
 * newlib syscall 适配层 (R52 / task_scheduler)
 *
 * 背景:链接时 crt0/__libc_init_array 会把 newlib stdio 拉进镜像,
 * 而 stdio 依赖 _close/_lseek/_read/_write 等系统调用。若缺失,
 * 链接器会从 libnosys.a 拉入 stub 并打印 "not implemented" 警告。
 * 这里提供强符号实现:
 *   - _write   重定向到 R52 UART(printf/puts 可直接使用)
 *   - _sbrk    指向 link.ld 的 .heap 段
 *   - 其余返回合理默认值
 */
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include "system.h"

#undef errno
extern int errno;

/* 与 ts_printf 保持一致:按当前核选择 UART */
static uint32_t syscall_uart_base(void)
{
    return (get_cpu_id() == 0) ? UART0_BASE : UART1_BASE;
}

/* 标准输出/错误重定向到 UART */
int _write(int file, char *ptr, int len)
{
    uint32_t uart_base = syscall_uart_base();
    int i;

    (void)file;
    for (i = 0; i < len; i++) {
        uart_putchar(uart_base, ptr[i]);
    }
    return len;
}

/* 无标准输入,恒返回 0(EOF) */
int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

/* 裸机堆:使用 link.ld 的 .heap 段 */
extern char __heap_start;
extern char __heap_end;
static char *heap_ptr;

void *_sbrk(int incr)
{
    char *prev_heap_end;

    if (heap_ptr == 0) {
        heap_ptr = &__heap_start;
    }
    prev_heap_end = heap_ptr;
    if (heap_ptr + incr > &__heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_ptr += incr;
    return prev_heap_end;
}

void _exit(int status)
{
    (void)status;
    while (1) {
    }
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

int _getpid(void)
{
    return 1;
}

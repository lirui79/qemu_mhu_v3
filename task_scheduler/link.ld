/* R52 内存布局(16MB 本地 RAM + 共享 DDR):
 * 1) 本地 RAM 16MB(0x00000000..0x00ffffff):
 *    - conf.lua ram_0 已扩到 16MB,R52 双核共享。
 *    - 代码(.text)+ .data(VMA=LMA)+ FreeRTOS 堆 + .bss + .heap + .stack
 *      全部放入本地 RAM,空间充足。
 * 2) 共享 DDR(0x80000000..0xa0000000,512MB)可读写:
 *    - startup.S MPU Region 5 已配置 DDR 为 Full access(RW)+ ENable。
 *    - A76 固件加载在 0x80100000..0x80500000(4MB bin),A76 ram 段
 *      0x80500000..0x80900000,A76 动态 DDR_BASE=0x81000000。
 *    - R52 的共享数据缓冲(BQueue 数据区等)放在 0x80900000 起 2MB,
 *      避开 A76 固件区与 A76 动态区,由 ddr_mem 模块(utils/ddr_mem.c)
 *      分配,减轻本地 RAM 堆压力。
 */
__RAM_BASE = 0x00000000;
__RAM_SIZE = 0x01000000;
__DDR_BUF_BASE = 0x80900000;
__DDR_BUF_SIZE = 0x00200000;

MEMORY
{
  RAM (rwx) : ORIGIN = __RAM_BASE, LENGTH = __RAM_SIZE
  DDR (rw)  : ORIGIN = __DDR_BUF_BASE, LENGTH = __DDR_BUF_SIZE
}


/* Linker script to place sections and symbol values.
 * It references following symbols, which must be defined in code:
 *   vectors : Entry point
 *
 * It defines following symbols, which code can use without definition:
 *   __code_start
 *   __exidx_start
 *   __exidx_end
 *   __data_start
 *   __data_end
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __bss_start__
 *   __bss_end__
 *   __stack
 */
ENTRY(Start)

SECTIONS
{
    .text 0x00000000 :
    {
        __code_start = .;
        *(VECTORS)
        KEEP (*(SORT_NONE(.init)))
        *(.text*)

        /* 独立段的启动/外设汇编(generictimer.S / v8rgicv3cpuif.S)。
         * 必须显式放在 RAM 内,否则会变成孤儿段被 ld 排到末尾,
         * 运行时取指到未初始化内存直接崩溃。 */
        *generictimer*
        *v8rgicv3cpuif*

        KEEP (*(SORT_NONE(.fini)))

        /* .ctors */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)

        /* .dtors */
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        *(.rodata .rodata.* .gnu.linkonce.r.*)

        KEEP (*(.eh_frame))
    } > RAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > RAM

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
        __code_end = .;
    } > RAM

    /* .data VMA=LMA,紧随代码放入本地 RAM 内;startup.S 的 memcpy 为
     * 自拷贝(dst==src),搬运逻辑保持不变 */
    __etext = ALIGN(8);

    .data : AT (__etext)
    {
        __data_start = . ;
        *(.data .data.* .gnu.linkonce.d.*)
        SORT(CONSTRUCTORS)

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        KEEP (*(.jcr*))
        __data_end = . ;
    } > RAM

    /* FreeRTOS 堆(heap_4 的 ucHeapPerCore,64KB/核,见 FreeRTOSConfig.h
     * configTOTAL_HEAP_SIZE)。放本地 RAM;该段须在 .bss 之前,ld 才会把
     * heap_4.o(.bss) 归入此段 */
    .frtos_heap (NOLOAD) :
    {
        . = ALIGN(8);
        __frtos_heap_start = .;
        *heap_4.o(.bss)
        *heap_4.o(COMMON)
        . = ALIGN(8);
        __frtos_heap_end = .;
    } > RAM

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(64);
        __bss_end__ = .;
    } > RAM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start = .;
        __end__ = .;
        PROVIDE(end = .);
        . = . + 0x400;
        __heap_end = .;
    } > RAM

    /* 双核栈:core1 在最高地址,向下增长;core0 紧随其后。 */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + 0x2000;
        __stack_core_0 = .;
        . = . + 0x2000;
        __stack_core_1 = .;
        __stack = .;
    } > RAM

    /* 共享数据缓冲(共享 DDR):BQueue 数据区等由 utils/ddr_mem.c 的
     * ddr_alloc() 从 __ddr_buf_start 起 bump 分配。NOLOAD:仅保留地址空间,
     * 不生成镜像内容,启动时无需拷贝。 */
    .ddr_buf (NOLOAD) :
    {
        . = ALIGN(64);
        __ddr_buf_start = .;
        . = . + __DDR_BUF_SIZE;
        __ddr_buf_end = .;
    } > DDR
}

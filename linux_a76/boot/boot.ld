/* SPDX-License-Identifier: GPL-2.0 */
ENTRY(_start)

SECTIONS
{
	. = 0x80100000;
	.text : { *(.text.boot) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
}

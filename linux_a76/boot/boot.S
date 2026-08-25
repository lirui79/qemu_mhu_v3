// SPDX-License-Identifier: GPL-2.0

.section .text.boot, "ax"
.global _start

.equ DTB_ADDRESS,   0x80110000
.equ IMAGE_ADDRESS, 0x80200000

_start:
	msr daifset, #0xf
	mov x1, xzr
	mov x2, xzr
	mov x3, xzr
	ldr x0, =DTB_ADDRESS
	ldr x4, =IMAGE_ADDRESS
	dsb sy
	isb
	br x4

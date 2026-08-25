/*
 * startup.S - AArch64 startup code for Cortex-A76
 * Handles EL3->EL2->EL1 transition, BSS init, vector table setup
 */

.section .text.boot
.global _start

_start:
    /* Read Current Exception Level */
    mrs x0, CurrentEL
    lsr x0, x0, #2
    and x0, x0, #3

    cmp x0, #3
    beq _el3_init
    cmp x0, #2
    beq _el2_init
    cmp x0, #1
    beq _el1_init

    /* Unknown EL - hang */
    b .

/*------------------------------------------------------------------------------
 * EL3 initialization: drop to EL2
 *----------------------------------------------------------------------------*/
_el3_init:
    /* SCR_EL3: NS=1, IRQ/FIQ/EA not routed to EL3, RW=1 (AArch64 for lower EL) */
    mov x1, #0x631
    msr scr_el3, x1

    /* SPSR_EL3: return to EL2h in AArch64, all masks cleared */
    mov x1, #0x3c9
    msr spsr_el3, x1

    adr x1, _el2_init
    msr elr_el3, x1

    dsb sy
    isb
    eret

/*------------------------------------------------------------------------------
 * EL2 initialization: drop to EL1
 *----------------------------------------------------------------------------*/
_el2_init:
    /* HCR_EL2: RW=1 (EL1 executes in AArch64) */
    mov x1, #(1 << 31)
    msr hcr_el2, x1

    /* CNTHCTL_EL2: allow EL1 access to physical timer (EL1PCEN=1, EL1PCTEN=1) */
    mov x1, #3
    msr cnthctl_el2, x1

    /* CPTR_EL2: don't trap FP/SIMD at EL1 */
    mov x1, #(3 << 20)
    msr cptr_el2, x1

    /* SPSR_EL2: return to EL1h in AArch64, DAIF masked */
    mov x1, #0x3c5
    msr spsr_el2, x1

    /* Ensure CNTVOFF_EL2 is zero (virtual timer == physical timer) */
    msr cntvoff_el2, xzr

    adr x1, _el1_init
    msr elr_el2, x1

    dsb sy
    isb
    eret

/*------------------------------------------------------------------------------
 * EL1 initialization
 *----------------------------------------------------------------------------*/
_el1_init:
    /* Use SP_EL1 for exception handling */
    msr SPSel, #1

    /* Initialize SP_EL1 stack pointer */
    ldr x0, =__stack_top
    mov sp, x0

    /* Zero BSS section */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, xzr
1:
    cmp x0, x1
    b.ge 2f
    str x2, [x0], #8
    b 1b
2:
    /* Install exception vector table at VBAR_EL1 */
    ldr x0, =vector_table
    msr vbar_el1, x0

    /* Enable FPU and SIMD at EL1 (CPACR_EL1.FPEN = 3) */
    mov x0, #(3 << 20)
    msr cpacr_el1, x0

    isb

    /* Jump to C boot entry */
    bl boot_main

    /* Should never return */
    b .

/*------------------------------------------------------------------------------
 * Hang loop
 *----------------------------------------------------------------------------*/
.global hang
hang:
    wfi
    b hang


/*==============================================================================
 * AArch64 Exception Vector Table (VBAR_EL1)
 * Each entry occupies 0x80 (128) bytes, 32 instruction slots
 *==============================================================================*/

.macro ventry label
    .balign 0x80
    b \label
.endm

.section .text.vector_table, "ax"
.balign 0x800
.global vector_table
.global _freertos_vector_table
vector_table:
_freertos_vector_table:

/*-------------------- Current EL with SP0 --------------------*/
    ventry el1t_sync_handler       /* Synchronous          +0x000 */
    ventry el1t_irq_handler        /* IRQ                  +0x080 */
    ventry el1t_fiq_handler        /* FIQ                  +0x100 */
    ventry el1t_serror_handler     /* SError               +0x180 */

/*-------------------- Current EL with SPx (SP_EL1) --------------------*/
    ventry el1h_sync_handler       /* Synchronous (SVC)    +0x200 */
    ventry el1h_irq_handler        /* IRQ (Timer, etc.)    +0x280 */
    ventry el1h_fiq_handler        /* FIQ                  +0x300 */
    ventry el1h_serror_handler     /* SError               +0x380 */

/*-------------------- Lower EL using AArch64 --------------------*/
    ventry el0_64_sync_handler     /* Synchronous          +0x400 */
    ventry el0_64_irq_handler      /* IRQ                  +0x480 */
    ventry el0_64_fiq_handler      /* FIQ                  +0x500 */
    ventry el0_64_serror_handler   /* SError               +0x580 */

/*-------------------- Lower EL using AArch32 --------------------*/
    ventry el0_32_sync_handler     /* Synchronous          +0x600 */
    ventry el0_32_irq_handler      /* IRQ                  +0x680 */
    ventry el0_32_fiq_handler      /* FIQ                  +0x700 */
    ventry el0_32_serror_handler   /* SError               +0x780 */


/*==============================================================================
 * Exception Handlers
 *
 * EL1h (Current EL with SP_EL1) handlers are the primary runtime handlers.
 * FreeRTOS uses:
 *   - el1h_sync_handler -> FreeRTOS_SWI_Handler (SVC for context switch)
 *   - el1h_irq_handler  -> FreeRTOS_IRQ_Handler (timer/UART interrupts)
 *==============================================================================*/

/*------------------------------------------------------------------------------
 * EL1h Synchronous handler - dispatches SVC to FreeRTOS
 *----------------------------------------------------------------------------*/
el1h_sync_handler:
    /* Read Exception Syndrome Register to determine cause */
    mrs x0, esr_el1
    lsr x1, x0, #26          /* Extract Exception Class (EC) */

    cmp x1, #0x15            /* SVC instruction from AArch64 */
    beq svc_64_handler

    /* Unhandled sync exception - dump info and hang */
    mrs x0, esr_el1
    mrs x1, elr_el1
    mrs x2, far_el1
    b unexpected_exception

svc_64_handler:
    /* Delegate to FreeRTOS SVC handler (defined in portasm.S) */
    b FreeRTOS_SWI_Handler

/*------------------------------------------------------------------------------
 * EL1h IRQ handler - delegates to FreeRTOS IRQ handler
 *----------------------------------------------------------------------------*/
el1h_irq_handler:
    b FreeRTOS_IRQ_Handler

/*------------------------------------------------------------------------------
 * EL1h FIQ handler
 *----------------------------------------------------------------------------*/
el1h_fiq_handler:
    b .

/*------------------------------------------------------------------------------
 * EL1h SError handler
 *----------------------------------------------------------------------------*/
el1h_serror_handler:
    b .

/*------------------------------------------------------------------------------
 * EL1t handlers — tasks run at EL1t (EL1 + SP_EL0), so IRQ and SVC
 * can arrive here during task execution.
 *----------------------------------------------------------------------------*/
el1t_sync_handler:
    /* Forward to EL1h sync handler (SVC dispatch) */
    b   el1h_sync_handler
el1t_irq_handler:
    /* Forward to FreeRTOS IRQ handler */
    b   FreeRTOS_IRQ_Handler
el1t_fiq_handler:
    b .
el1t_serror_handler:
    b .

/*------------------------------------------------------------------------------
 * EL0 AArch64 handlers (unused in this design)
 *----------------------------------------------------------------------------*/
el0_64_sync_handler:
    b .
el0_64_irq_handler:
    b .
el0_64_fiq_handler:
    b .
el0_64_serror_handler:
    b .

/*------------------------------------------------------------------------------
 * EL0 AArch32 handlers (unused)
 *----------------------------------------------------------------------------*/
el0_32_sync_handler:
    b .
el0_32_irq_handler:
    b .
el0_32_fiq_handler:
    b .
el0_32_serror_handler:
    b .

/*------------------------------------------------------------------------------
 * Unexpected exception - save regs and hang
 *----------------------------------------------------------------------------*/
unexpected_exception:
    /* x0 = ESR_EL1, x1 = ELR_EL1, x2 = FAR_EL1 */
    mov x3, #0xD0000000
    /* Store exception info at known location for debug */
    str x0, [x3, #0x1000]
    str x1, [x3, #0x1008]
    str x2, [x3, #0x1010]
    b hang

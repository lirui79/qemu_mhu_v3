//----------------------------------------------------------------
// Copyright (c) 2016-2023 Arm Limited (or its affiliates). All rights reserved.
// Use, modification and redistribution of this file is subject to your possession of a
// valid End User License Agreement for the Arm Product of which these examples are part of
// and your compliance with all applicable terms and conditions of such license agreement.
//----------------------------------------------------------------


.equ Mode_SVC, 0x13
.equ SPURIOUSID, 0x3FF

    .section  exception_handlers,"ax"
#ifdef __THUMB__
    .thumb
#endif
    .global EL1_IRQ_Handler
    .type EL1_IRQ_Handler, %function
EL1_IRQ_Handler:    
    sub lr, lr, #4                      // Perform lr adjustment to preferred return address: legacy
    srsfd sp!, #Mode_SVC                // Save SPSR_irq and lr_irq into the SVC stack
    cps #Mode_SVC                       // Change to SVC mode
    push {r0-r4, r12}                   // Stack registers onto the SVC stack
    and r1, sp, #4                      // Check if the sp is 8-byte aligned, if not r1 = 4. AAPCS mandates 8-byte alignment at all external boundaries (i.e. separately compiled or assembled files).
    sub sp, sp, r1                      // Sub 0 (already aligned) or 4 (misaligned)
    push {r1, lr}                       // Push r1 (adjustment info) and lr_svc onto the stack. lr_svc is used in the subroutine call, so it must be stacked (here) and restored (before returning).
    bl readAliasedIntAck                // r0 will contain the IRQ source number, source identified, interrupt active
1:    mov r4, r0                        // Use r4 to store the IRQ source number, to be used later
    cpsie i                             // Re-enable interrupts (IRQ)
    bl  irq_handler                     // Branch to the C Interrupt Service Routine
    cpsid i                             // Disable interrupts (IRQ)
    mov r0, r4                          // Move IRQ source number (stored in r4) to r0
    bl writeAliasedEOI                  // Write r0 to ICC_EOIR: End of interrupt, priority drop and deactivation
    bl readAliasedIntAck                // Before restoring the context, check if any other interrupt is pending
    mov r1, #SPURIOUSID                 // 1023: spurious interrupt, meaning no interrupt pending
    cmp r0, r1                          // Compare with what was read
    bne 1b                              // If there is another interrupt pending, handle it
    pop {r1, lr}                        // Pop r1 (adjustment info) and lr_svc from the stack
    add sp, sp, r1                      // Unadjust sp as per previous state
    pop {r0-r4, r12}                    // Pop the rest of the AAPCS registers to restore the context
    rfefd sp!                           // Return from exception by restoring the pc from the lr_irq and the CPSR from SPSR_irq saved in the SVC stack

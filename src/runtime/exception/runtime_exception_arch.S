.section .text.runtime_exception,"ax",@progbits
.arch_extension pauth
.p2align 2
.global avp_exception_entry
.type avp_exception_entry,@function
avp_exception_entry:
    /* SP_EL1 is initialized once and retained across EL0 task switches. */
    sub sp, sp, #0x20
    stp x16, x17, [sp, #0x00]
    stp x18, x19, [sp, #0x10]
    mrs x16, esr_el1
    lsr x17, x16, #26
    cmp x17, #0x15
    b.ne .Llegacy_exception
    and w17, w16, #0xffff
    cmp w17, #0x20
    b.eq .Lsave_svc_exception
    cmp w17, #0x22
    b.eq .Lsave_svc_exception
    cmp w17, #0x23
    b.eq .Lsave_svc_exception
    cmp w17, #0x40
    b.eq .Lsave_svc_exception
    cmp w17, #0x41
    b.eq .Lsave_svc_exception
    cmp w17, #0x42
    b.eq .Lsave_svc_exception
    cmp w17, #0x43
    b.eq .Lsave_svc_exception
    cmp w17, #0x44
    b.eq .Lsave_svc_exception
    cmp w17, #0x45
    b.eq .Lsave_svc_exception
    cmp w17, #0x50
    b.eq .Lsave_svc_exception
    cmp w17, #0x51
    b.eq .Lsave_svc_exception
    cmp w17, #0x52
    b.eq .Lsave_svc_exception
    cmp w17, #3
    b.eq .Lhandoff_to_el1
    cmp w17, #0xc2
    b.eq .Lsave_svc_exception
    cmp w17, #0xc3
    b.eq .Lsave_svc_exception
    cmp w17, #0xc4
    b.eq .Lsave_svc_exception
    cmp w17, #0xc5
    b.ne .Llegacy_exception

.Lsave_svc_exception:
    mov x19, xzr
    b .Lsave_exception

.p2align 2
.global avp_timer_fiq_entry
.type avp_timer_fiq_entry,@function
avp_timer_fiq_entry:
    sub sp, sp, #0x20
    stp x16, x17, [sp, #0x00]
    stp x18, x19, [sp, #0x10]
    mov x19, #1

.Lsave_exception:
    mrs x16, tpidr_el1
    cbz x16, pvpanic_trigger
    stp x0, x1, [x16, #0x00]
    stp x2, x3, [x16, #0x10]
    stp x4, x5, [x16, #0x20]
    stp x6, x7, [x16, #0x30]
    stp x8, x9, [x16, #0x40]
    stp x10, x11, [x16, #0x50]
    stp x12, x13, [x16, #0x60]
    stp x14, x15, [x16, #0x70]
    ldr x17, [sp, #0x00]
    str x17, [x16, #0x80]
    ldr x17, [sp, #0x08]
    str x17, [x16, #0x88]
    ldr x17, [sp, #0x10]
    str x17, [x16, #0x90]
    ldr x17, [sp, #0x18]
    str x17, [x16, #0x98]
    stp x20, x21, [x16, #0xa0]
    stp x22, x23, [x16, #0xb0]
    stp x24, x25, [x16, #0xc0]
    stp x26, x27, [x16, #0xd0]
    stp x28, x29, [x16, #0xe0]
    str x30, [x16, #0xf0]

    mrs x17, elr_el1
    paciza x17
    str x17, [x16, #0xf8]
    mrs x17, sp_el0
    str x17, [x16, #0x100]
    mrs x17, spsr_el1
    str x17, [x16, #0x108]
    mrs x17, fpsr
    str x17, [x16, #0x110]
    mrs x17, fpcr
    str x17, [x16, #0x118]
    mrs x17, tpidr_el0
    str x17, [x16, #0x120]
    mrs x17, tpidrro_el0
    str x17, [x16, #0x128]
    mrs x17, ttbr1_el1
    str x17, [x16, #0x130]
    mrs x17, ttbr0_el1
    str x17, [x16, #0x360]
    adrp x17, __stack_chk_guard
    add x17, x17, :lo12:__stack_chk_guard
    ldr x17, [x17]
    str x17, [x16, #0x340]
    cbnz x19, .Lsave_full_vectors
    strh wzr, [x16, #0x138]
    stp q8, q9, [x16, #0x1c0]
    stp q10, q11, [x16, #0x1e0]
    stp q12, q13, [x16, #0x200]
    stp q14, q15, [x16, #0x220]
    b .Lvectors_saved

.Lsave_full_vectors:
    mov w17, #1
    strh w17, [x16, #0x138]
    stp q0, q1, [x16, #0x140]
    stp q2, q3, [x16, #0x160]
    stp q4, q5, [x16, #0x180]
    stp q6, q7, [x16, #0x1a0]
    stp q8, q9, [x16, #0x1c0]
    stp q10, q11, [x16, #0x1e0]
    stp q12, q13, [x16, #0x200]
    stp q14, q15, [x16, #0x220]
    stp q16, q17, [x16, #0x240]
    stp q18, q19, [x16, #0x260]
    stp q20, q21, [x16, #0x280]
    stp q22, q23, [x16, #0x2a0]
    stp q24, q25, [x16, #0x2c0]
    stp q26, q27, [x16, #0x2e0]
    stp q28, q29, [x16, #0x300]
    stp q30, q31, [x16, #0x320]

.Lvectors_saved:

    cbnz x19, .Ldispatch_timer_fiq
    mov x0, x16
    mrs x1, esr_el1
    and w1, w1, #0xffff
    bl runtime_exception_dispatch
    b .Lpublish_selected_task
.Ldispatch_timer_fiq:
    mov x0, x16
    bl runtime_irq_system_timer_fiq_from_exception
.Lpublish_selected_task:
    mov x16, x0
    cbz x16, pvpanic_trigger
    msr tpidr_el1, x16

    ldr x17, [x16, #0xf8]
    autiza x17
    msr elr_el1, x17
    ldr x17, [x16, #0x100]
    msr sp_el0, x17
    ldr x17, [x16, #0x108]
    msr spsr_el1, x17
    ldr x17, [x16, #0x110]
    msr fpsr, x17
    ldr x17, [x16, #0x118]
    msr fpcr, x17
    ldr x17, [x16, #0x120]
    msr tpidr_el0, x17
    ldr x17, [x16, #0x128]
    msr tpidrro_el0, x17
    ldr x17, [x16, #0x340]
    adrp x18, __stack_chk_guard
    add x18, x18, :lo12:__stack_chk_guard
    str x17, [x18]
    ldrh w17, [x16, #0x13a]
    strh wzr, [x16, #0x13a]
    cbnz w17, .Laddress_space_restored
    /* Matching vresearch1 uses TTBR1_EL1 for the scheduled address space. */
    ldr x17, [x16, #0x130]
    mrs x18, ttbr1_el1
    msr ttbr1_el1, x17
    cmp x17, x18
    b.eq .Laddress_space_restored
    mov x18, #0xffff000000000000
    and x17, x17, x18
    dsb sy
    tlbi aside1, x17
    dsb sy
    isb
.Laddress_space_restored:
    ldrh w17, [x16, #0x138]
    strh wzr, [x16, #0x138]
    cbnz w17, .Lfull_vector_restore
    ldp q8, q9, [x16, #0x1c0]
    ldp q10, q11, [x16, #0x1e0]
    ldp q12, q13, [x16, #0x200]
    ldp q14, q15, [x16, #0x220]
    movi v0.16b, #0
    movi v1.16b, #0
    movi v2.16b, #0
    movi v3.16b, #0
    movi v4.16b, #0
    movi v5.16b, #0
    movi v6.16b, #0
    movi v7.16b, #0
    movi v16.16b, #0
    movi v17.16b, #0
    movi v18.16b, #0
    movi v19.16b, #0
    movi v20.16b, #0
    movi v21.16b, #0
    movi v22.16b, #0
    movi v23.16b, #0
    movi v24.16b, #0
    movi v25.16b, #0
    movi v26.16b, #0
    movi v27.16b, #0
    movi v28.16b, #0
    movi v29.16b, #0
    movi v30.16b, #0
    movi v31.16b, #0
    ldp x18, x19, [x16, #0x90]
    ldp x20, x21, [x16, #0xa0]
    ldp x22, x23, [x16, #0xb0]
    ldp x24, x25, [x16, #0xc0]
    ldp x26, x27, [x16, #0xd0]
    ldp x28, x29, [x16, #0xe0]
    ldr x30, [x16, #0xf0]
    ldp x0, x1, [x16, #0x00]
    ldp x2, x3, [x16, #0x10]
    ldp x4, x5, [x16, #0x20]
    ldp x6, x7, [x16, #0x30]
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mov x12, xzr
    mov x13, xzr
    mov x14, xzr
    mov x15, xzr
    mov x16, xzr
    mov x17, xzr
    add sp, sp, #0x20
    eret

.Lfull_vector_restore:
    ldp q0, q1, [x16, #0x140]
    ldp q2, q3, [x16, #0x160]
    ldp q4, q5, [x16, #0x180]
    ldp q6, q7, [x16, #0x1a0]
    ldp q8, q9, [x16, #0x1c0]
    ldp q10, q11, [x16, #0x1e0]
    ldp q12, q13, [x16, #0x200]
    ldp q14, q15, [x16, #0x220]
    ldp q16, q17, [x16, #0x240]
    ldp q18, q19, [x16, #0x260]
    ldp q20, q21, [x16, #0x280]
    ldp q22, q23, [x16, #0x2a0]
    ldp q24, q25, [x16, #0x2c0]
    ldp q26, q27, [x16, #0x2e0]
    ldp q28, q29, [x16, #0x300]
    ldp q30, q31, [x16, #0x320]
    ldp x17, x18, [x16, #0x80]
    stp x17, x18, [sp, #0x00]
    ldp x18, x19, [x16, #0x90]
    ldp x20, x21, [x16, #0xa0]
    ldp x22, x23, [x16, #0xb0]
    ldp x24, x25, [x16, #0xc0]
    ldp x26, x27, [x16, #0xd0]
    ldp x28, x29, [x16, #0xe0]
    ldr x30, [x16, #0xf0]
    ldp x0, x1, [x16, #0x00]
    ldp x2, x3, [x16, #0x10]
    ldp x4, x5, [x16, #0x20]
    ldp x6, x7, [x16, #0x30]
    ldp x8, x9, [x16, #0x40]
    ldp x10, x11, [x16, #0x50]
    ldp x12, x13, [x16, #0x60]
    ldp x14, x15, [x16, #0x70]
    ldp x16, x17, [sp, #0x00]
    add sp, sp, #0x20
    eret

.Llegacy_exception:
    ldp x18, x19, [sp, #0x10]
    ldp x16, x17, [sp, #0x00]
    add sp, sp, #0x20
    b avp_synchronous_exception
.Lhandoff_to_el1:
    ldp x18, x19, [sp, #0x10]
    ldp x16, x17, [sp, #0x00]
    add sp, sp, #0x20
    msr daifset, #0xf

    /* Match the AVPBooter-compatible SVC3 ABI and keep the payload entry out
     * of LR until the architectural scrub has completed. */
    mov x29, x0
    mov x27, x0
    mov x28, x1

    ldr x0, =0x70028000
    ldr x1, =0x70034000
.Lhandoff_zero_70028000:
    dc zva, x0
    add x0, x0, #0x40
    cmp x0, x1
    b.lo .Lhandoff_zero_70028000

    /* The normal path places illb at 0x70074000 and scrubs the
     * preceding owner window. Source DFU fixtures and iBSS occupy that
     * window, so their active image owns it until the branch completes. */
    ldr x0, =0x70074000
    cmp x27, x0
    b.lo .Lhandoff_preserve_payload_window
    ldr x0, =0x70064000
    ldr x1, =0x70074000
.Lhandoff_zero_70064000:
    dc zva, x0
    add x0, x0, #0x40
    cmp x0, x1
    b.lo .Lhandoff_zero_70064000
.Lhandoff_preserve_payload_window:

    ldr x0, =0x70034000
    ldr x1, =0x70040000
.Lhandoff_zero_70034000:
    dc zva, x0
    add x0, x0, #0x40
    cmp x0, x1
    b.lo .Lhandoff_zero_70034000

    ldr x0, =0x70050000
    ldr x1, =0x70054000
.Lhandoff_zero_70050000:
    dc zva, x0
    add x0, x0, #0x40
    cmp x0, x1
    b.lo .Lhandoff_zero_70050000

    dsb sy
    isb
    msr sctlr_el1, xzr
    dsb sy
    isb
    tlbi vmalle1
    dsb sy
    isb

    ldr x0, =0x7004c000
    ldr x1, =0x70050000
.Lhandoff_zero_7004c000:
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    cmp x0, x1
    b.lo .Lhandoff_zero_7004c000

    ldr x0, =0x70054000
    ldr x1, =0x70058000
.Lhandoff_zero_70054000:
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    cmp x0, x1
    b.lo .Lhandoff_zero_70054000

    ldr x0, =0x70000000
    ldr x1, =0x70028000
.Lhandoff_zero_70000000:
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    cmp x0, x1
    b.lo .Lhandoff_zero_70000000

    ldr x0, =0x70058000
    ldr x1, =0x70064000
.Lhandoff_zero_70058000:
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    stp xzr, xzr, [x0], #0x10
    cmp x0, x1
    b.lo .Lhandoff_zero_70058000

    tlbi vmalle1
    dsb sy
    isb

    mov x3, xzr
    mov x4, xzr
    mov x5, xzr
    mov x6, xzr
    mov x7, xzr
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mov x12, xzr
    mov x13, xzr
    mov x14, xzr
    mov x15, xzr
    mov x16, xzr
    mov x17, xzr
    mov x18, xzr
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr
    mov x27, xzr

    /* Apple's resident handoff trampoline enters the payload with a null
     * boot argument and clears the source runtime's register ownership. */
    movi v0.16b, #0
    movi v1.16b, #0
    movi v2.16b, #0
    movi v3.16b, #0
    movi v4.16b, #0
    movi v5.16b, #0
    movi v6.16b, #0
    movi v7.16b, #0
    movi v8.16b, #0
    movi v9.16b, #0
    movi v10.16b, #0
    movi v11.16b, #0
    movi v12.16b, #0
    movi v13.16b, #0
    movi v14.16b, #0
    movi v15.16b, #0
    movi v16.16b, #0
    movi v17.16b, #0
    movi v18.16b, #0
    movi v19.16b, #0
    movi v20.16b, #0
    movi v21.16b, #0
    movi v22.16b, #0
    movi v23.16b, #0
    movi v24.16b, #0
    movi v25.16b, #0
    movi v26.16b, #0
    movi v27.16b, #0
    movi v28.16b, #0
    movi v29.16b, #0
    movi v30.16b, #0
    movi v31.16b, #0

    msr elr_el1, xzr
    msr par_el1, xzr
    msr tcr_el1, xzr
    msr tpidr_el0, xzr
    msr tpidrro_el0, xzr
    msr tpidr_el1, xzr
    msr ttbr0_el1, xzr
    msr ttbr1_el1, xzr
    msr vbar_el1, xzr
    msr spsr_el1, xzr

    mov x2, xzr
    msr spsel, #0
    mov sp, x2
    msr spsel, #1
    mov sp, x2
    mov x30, x29
    mov x29, xzr
    mov x0, x28
    mov x1, xzr
    mov x28, xzr
    /*
     * Payloads built for the stock t8110 firmware family are entered at EL2
     * and their first instruction reads HCR_EL2, so climb before branching
     * when the board offers it. start.S published that; the read is a plain
     * physical load because SCTLR_EL1 was already cleared above. x30 carries
     * the entry and x0 the payload's argument, so hand the entry over in x4
     * and let the EL2 vector return into it.
     */
    ldr x2, =avp_el2_present
    ldrb w2, [x2]
    cbz w2, .Lhandoff_enter_payload
    mov x4, x30
    mov x2, xzr
    hvc #0
.Lhandoff_enter_payload:
    mov x2, xzr
    ic iallu
    dsb sy
    isb
    ret
.size avp_exception_entry, .-avp_exception_entry
.size avp_timer_fiq_entry, .-avp_timer_fiq_entry

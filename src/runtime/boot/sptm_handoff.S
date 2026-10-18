/*
 * A place to stand between iBoot and whatever comes next.
 *
 * iBoot's handoff trampoline copies itself out of the image to a scratch page
 * so it can erase the image it came from, masks interrupts, turns the MMU
 * off, cleans every cache level, and then jumps to the next stage.  Only what
 * is inside that copy still exists by the time it runs, and the tail of it is
 * patched to come here.
 *
 * What that buys is a moment with the machine in a state that is hard to
 * arrange anywhere else: EL2, MMU off, so every address is the physical one
 * and the device tree and the boot arguments can be reached without a
 * translation regime; caches already clean, so nothing has to be written back
 * on the way out.  iBoot has no source, and what has to happen here is known,
 * so it happens in code we build.
 *
 * The contract is narrow.  X0 already carries the handoff argument and X30 is
 * the next stage; neither may change.  There is no stack -- SPSel was toggled
 * and SP_EL1 was never established -- so one is borrowed out of the ROM
 * aperture before anything else runs.  The patch lands two words before the
 * trampoline's RET, which means the rest of the epilogue never runs and this
 * has to finish it: X1 and X28 zeroed, the instruction cache invalidated, and
 * the barriers, before returning.
 *
 * Everything above that is in C, next door in sptm_handoff_tree.c.  An Apple
 * device tree is walked rather than indexed and every edit is conditional on
 * finding something, which is a poor fit for a register-only routine: the
 * version of this that searched the tree by scanning for name words gave up
 * on the whole handoff when one of those searches came up empty, and on
 * 15.0.1 -- whose tree has no sgx node -- that silently skipped the kernel
 * patches, the tree edits and the boot arguments alike.
 */

/*
 * Whether a kernel with no monitor over it is entered at EL1 rather than
 * returned to at EL2.  See the epilogue for what each costs.
 */
    .set    ENTER_KERNEL_AT_EL1, 1

    .set    SCRATCH,        0x00160000  /* ROM aperture, above the image  */
    .set    STACK,          0x001c0000  /* above the image, below 0x200000 */

    .section .handoff,"ax",@progbits
    .p2align 4
    .global sptm_handoff_prepare
    .type sptm_handoff_prepare,@function
sptm_handoff_prepare:
    /* Say we were here, in the one megabyte nothing else has a use for. */
    movz    x1, #(SCRATCH >> 16), lsl #16
    movz    x2, #0xb007
    movk    x2, #0xb007, lsl #16
    /*
     * Which handoff this is.  The same two words divert both of them now --
     * iBSS and iBEC are one binary, so a patch table cannot tell them apart --
     * and the mark left here on the way through is what does: absent, this is
     * iBSS about to enter iBEC and the image it is entering is the one that
     * needs the table; present, iBEC is entering the next stage and everything
     * below applies.
     */
    ldr     x3, [x1]
    cmp     x3, x2
    b.eq    1f
    str     x2, [x1]
    mov     x19, x0
    mov     x20, x30
    movz    x23, #(STACK >> 16), lsl #16
    mov     sp, x23
    mov     x0, x20
    bl      ibec_handoff_patch
    mov     x0, x19
    mov     x30, x20
    mov     w22, #1                     /* nothing to decide on this pass  */
    b       9f

1:
    str     x2, [x1]
    mov     x19, x0                     /* the handoff argument           */
    mov     x20, x30                    /* where iBoot is about to go     */
    mov     x21, x1                     /* the scratch page               */
    movz    x23, #(STACK >> 16), lsl #16
    mov     sp, x23
    mov     x0, x19
    mov     x1, x21
    mov     x2, x20
    bl      sptm_handoff_run
    mov     x22, x0                     /* nonzero: a monitor is next     */
    mov     x0, x19
    mov     x30, x20

    /*
     * Tell the hypervisor the monitor is next.
     *
     * SPTM hands TXM over with MSR ELR_GL1 / MSR SPSR_GL1 and an ERET.  Those
     * writes are never trapped, so the only way anything outside can put that
     * pair where a hardware ERET looks for it is to trap the ERET, and that
     * needs HCR_EL2.NV -- which by here is off, because the guest's last
     * return to its own EL1 cleared it and nothing since has been an ERET.
     *
     * This is the moment that knows: the next instruction after the epilogue
     * is the monitor's.  One write to a register nothing else uses says so.
     *
     * Only when there is a monitor.  15.0.1 has none -- iBoot's bootx jumps
     * straight into the kernelcache -- and arming a trap for a guarded ERET
     * that never comes leaves the kernel running with NV set.
     */
    cbz     w22, 2f
    msr     s3_6_c15_c15_7, xzr
2:

    /*
     * Hand the timers over disarmed.
     *
     * Measured at the kernel's first instruction: CNTP_CVAL still held a value
     * from before the handoff -- and this machine keeps that register in
     * *host* counter units -- so it read as already expired, with the banked
     * control word's ISTATUS set.  Nothing fires while ENABLE is clear, but
     * the moment the kernel turns the timer on, before IOKit has published
     * the object its interrupt dispatch goes through, the condition is
     * already met and it takes one immediately.  That lands in sleh_fiq
     * dereferencing a null global, and no interrupt controller and no
     * software injection path is involved -- which is why it looked for a
     * long time as though no interrupt had been delivered at all.
     *
     * Real firmware leaves nothing armed here either.
     */
    msr     cntp_ctl_el0, xzr
    msr     cntv_ctl_el0, xzr
    msr     cntp_cval_el0, xzr
    msr     cntv_cval_el0, xzr
    isb

    /* What the epilogue would have done from the word this displaced. */
9:  dsb     sy
    mov     x1, #0
    mov     x28, #0
    ic      iallu
    dsb     sy
    isb
    .if ENTER_KERNEL_AT_EL1
    cbz     w22, 3f
    .endif
    ret

    /*
     * A kernel is entered at EL1, the way a phone enters one.
     *
     * iBoot's trampoline returns rather than eret-ing, so whatever exception
     * level it was at is the one the next stage gets -- and here that is EL2,
     * because everything from the ROM onwards runs there.  For 17.0 that is
     * right: SPTM is a monitor and belongs at EL2.  For 15.0.1 it is not.
     * XNU then finds CurrentEL == 8 and takes the EL2 arm of every choice it
     * makes, and one of those choices is which guarded per-CPU register holds
     * its own block: TPIDR_GL2 at EL2, TPIDR_GL1 below.  Nothing on this
     * machine carries TPIDR_GL2 -- the framework declines it and the private
     * context has no slot for it, while TPIDR_GL1 is carried with the rest of
     * the GL1 bank -- so the value the kernel writes there survives only until
     * the vCPU is next descheduled.  Measured: ml_get_ppl_cpu_data returning
     * zero inside PPL, and pmap_enter_options_internal then bzero-ing from a
     * null per-CPU block, at the same instruction on three runs out of three.
     *
     * Dropping to EL1 here is not a workaround for that; it is what the real
     * boot does.  A phone's iBoot enters the kernelcache at EL1 and iOS 15 has
     * no monitor above it.  The guarded bank the kernel then uses is the one
     * this machine can keep.
     *
     * Off, though, because it does not yet pay.  Measured: at EL1 the kernel
     * reaches CTRR lockdown and then takes a guarded abort into
     * _gtr_deadloop -- identically with the interrupt gates on and off, so
     * not a level delivered in guarded state -- while returning at EL2
     * reaches IOKit, forty-one threads and AppleARMBacklight before it stops
     * on the missing TPIDR_GL2.  Further wins until the abort is understood.
     */
    .if ENTER_KERNEL_AT_EL1
3:  msr     elr_el2, x30
    movz    x1, #0x3c5                  /* EL1h, DAIF masked               */
    msr     spsr_el2, x1
    mov     x1, #0
    isb
    eret
    .endif
    .size sptm_handoff_prepare, . - sptm_handoff_prepare

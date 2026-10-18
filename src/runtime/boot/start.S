.section .text.start,"ax",@progbits
.p2align 7
.global _start
.type _start,@function
_start:
    /*
     * When the board exposes EL2 the core resets into it. This ROM's runtime
     * lives at EL1 and runs tasks at EL0, so it drops straight back down and
     * stays there; all that is kept of EL2 is a vector table, so the final
     * handoff can climb back up. The stock t8110 firmware family is entered
     * at EL2 -- that SecureROM's reset vector is `mrs x2, HCR_EL2` and the
     * d16 iBSS opens with the same five instructions -- so the payload has to
     * arrive there, even though this ROM does not run there.
     *
     * w19 carries "EL2 is available" across the drop; it survives the .data
     * copy and .bss wipe below and is published once those are done.
     */
    mrs x0, CurrentEL
    cmp x0, #8
    cset w19, eq
    b.ne 9f
    adr x0, avp_el2_vector_table
    msr vbar_el2, x0
    mrs x0, hcr_el2
    bic x0, x0, #(1 << 34)      /* E2H: EL1 runtime uses standard semantics */
    bic x0, x0, #(1 << 27)      /* TGE: route EL1 exceptions to EL1 */
    orr x0, x0, #(1 << 31)      /* RW: EL1 is AArch64 */
    /*
     * Once EL2 exists it traps EL1's pointer authentication keys and PAuth
     * instructions unless APK and API say otherwise.
     */
    orr x0, x0, #(3 << 40)      /* APK | API */
    msr hcr_el2, x0
    mrs x0, cptr_el2
    bic x0, x0, #(1 << 10)      /* TFP: allow EL1 FP/SIMD state access */
    orr x0, x0, #(3 << 20)      /* FPEN: VHE CPTR_EL2 layout */
    msr cptr_el2, x0
    isb
    adr x0, 9f
    msr elr_el2, x0
    mov x0, #0x3c5              /* EL1h, DAIF masked */
    msr spsr_el2, x0
    isb
    eret
9:
    /* EL1 traps retain this bank while EL0 tasks own SP_EL0. */
    msr spsel, #1
    ldr x0, =__stack_top
    mov sp, x0
    /* Task context switches preserve Q8-Q15, so FP/SIMD must be enabled first. */
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    isb
    /* Runtime tasks build absolute deadlines from the architectural counters. */
    mrs x0, cntkctl_el1
    orr x0, x0, #1
    orr x0, x0, #0x200
    orr x0, x0, #0x100
    orr x0, x0, #2
    msr cntkctl_el1, x0
    isb
    adr x1, avp_vector_table
    msr vbar_el1, x1
    isb

    ldr x0, =__data_load_start
    ldr x1, =__data_start
    ldr x2, =__data_end
1:
    cmp x1, x2
    b.hs 2f
    ldrb w3, [x0], #1
    strb w3, [x1], #1
    b 1b
2:
    ldr x1, =__bss_start
    ldr x2, =__bss_end
3:
    cmp x1, x2
    b.hs 4f
    strb wzr, [x1], #1
    b 3b
4:
    ldr x1, =__runtime_auxiliary_start
    ldr x2, =__runtime_auxiliary_end
6:
    cmp x1, x2
    b.hs 7f
    strb wzr, [x1], #1
    b 6b
7:
    /* Past the .bss wipe, so this survives into the handoff. */
    adrp x0, avp_el2_present
    add x0, x0, :lo12:avp_el2_present
    strb w19, [x0]
    /* The runtime installs its 16 KiB identity root before C state. */
    bl runtime_mmu_initialize
    bl runtime_initialize_bootstrap_stack_guard
    bl firmware_main
5:
    wfe
    b 5b
.size _start, .-_start

.macro avp_vector_entry target
    b \target
    .rept 31
        nop
    .endr
.endm

.p2align 11
.global avp_vector_table
avp_vector_table:
    .rept 4
        avp_vector_entry pvpanic_trigger
    .endr
    avp_vector_entry avp_exception_entry
    avp_vector_entry pvpanic_trigger
    avp_vector_entry avp_timer_fiq_entry
    avp_vector_entry pvpanic_trigger
    avp_vector_entry avp_exception_entry
    avp_vector_entry pvpanic_trigger
    avp_vector_entry avp_timer_fiq_entry
    .rept 5
        avp_vector_entry pvpanic_trigger
    .endr

/* Unrecognized synchronous exceptions remain fatal. */
.p2align 2
.global avp_synchronous_exception
.type avp_synchronous_exception,@function
avp_synchronous_exception:
    b pvpanic_trigger
.size avp_synchronous_exception, .-avp_synchronous_exception

/*
 * EL2 vector table. The only entry ever taken is the synchronous one for a
 * lower exception level: runtime_boot_enter_payload_at_el2 issues HVC from
 * EL1 to climb back up for the handoff. Everything else that arrives here is
 * unexpected and parks rather than running off into whatever follows.
 */
.section .text.el2,"ax",@progbits
.p2align 11
.global avp_el2_vector_table
avp_el2_vector_table:
    /* Current EL with SP_EL0. */
    .p2align 7
.Lel2_cur_sp0_sync:
    mov x12, #0
    b .Lel2_record
    .p2align 7
.Lel2_cur_sp0_irq:
    mov x12, #1
    b .Lel2_record
    .p2align 7
.Lel2_cur_sp0_fiq:
    mov x12, #2
    b .Lel2_record
    .p2align 7
.Lel2_cur_sp0_serr:
    mov x12, #3
    b .Lel2_record
    /* Current EL with SP_ELx. */
    .p2align 7
.Lel2_cur_spx_sync:
    mov x12, #4
    b .Lel2_record
    .p2align 7
.Lel2_cur_spx_irq:
    mov x12, #5
    b .Lel2_record
    .p2align 7
.Lel2_cur_spx_fiq:
    mov x12, #6
    b .Lel2_record
    .p2align 7
.Lel2_cur_spx_serr:
    mov x12, #7
    b .Lel2_record
    /* Lower EL, AArch64: the handoff HVC arrives here. */
    .p2align 7
.Lel2_low64_sync:
    /*
     * Breadcrumbs into the ROM aperture, written at EL2 with the MMU off so
     * they are plain physical stores: 0xe1 means a synchronous exception from
     * a lower level arrived at all, 0xe2 that it was the handoff HVC.
     */
    mov x6, #0xffe0
    movk x6, #0x17, lsl #16
    mov x7, #0xe1
    strb w7, [x6]
    mrs x5, esr_el2
    lsr x5, x5, #26
    cmp x5, #0x16               /* EC == HVC64 */
    mov x12, #8
    b.ne .Lel2_record
    mov x7, #0xe2
    strb w7, [x6, #1]
    /* x0-x3 are the payload arguments, x4 its entry point. */
    msr elr_el2, x4
    mov x5, #0x3c9              /* EL2h, DAIF masked */
    msr spsr_el2, x5

    /*
     * E2H|TGE before the translation registers, because their layout depends
     * on it and this is the state the payload itself selects in its first
     * five instructions.
     */
    mrs x5, hcr_el2
    orr x5, x5, #(1 << 34)      /* E2H */
    orr x5, x5, #(1 << 27)      /* TGE */
    msr hcr_el2, x5
    isb

    /*
     * HVF exposes the host's 16 KiB translation granule to this VHE guest.
     * iBSS enables SCTLR_EL2.M before replacing TTBR0_EL2, so its inherited
     * root must already use that format. One level-1 table descriptor and a
     * full level-2 block table identity-map the 64 GiB machine aperture.
     */
    mov x5, #0
    movk x5, #0x19, lsl #16     /* 16 KiB-aligned level-1 root */
    mov x6, #0x4000
    movk x6, #0x19, lsl #16     /* level-2 table at 0x194000 */
    orr x7, x6, #3              /* valid table descriptor */
    str x7, [x5]
    add x7, x5, #8
    add x8, x5, #0x4000
.Lel2_map_clear_root:
    str xzr, [x7], #8
    cmp x7, x8
    b.lo .Lel2_map_clear_root

    mov x7, xzr
.Lel2_map_block:
    lsl x8, x7, #25             /* 16 KiB level-2 blocks are 32 MiB */
    mov x9, #0x705              /* AF | inner shareable | attr1 | block */
    sub x10, x7, #256           /* 0x2_0000_0000..0x2_ffff_ffff */
    cmp x10, #128
    b.hs .Lel2_map_store
    mov x9, #0x409              /* AF | attr2 | block */
.Lel2_map_store:
    orr x8, x8, x9
    str x8, [x6, x7, lsl #3]
    add x7, x7, #1
    cmp x7, #2048
    b.lo .Lel2_map_block

    /*
     * The same attribute encoding the payload installs once it takes
     * over: attr0 Device-nGnRE, attr1 Normal write-back, attr2
     * Device-nGnRnE, attr3 Normal non-cacheable.  It writes MAIR before it
     * installs its own tables, so ours are still the ones in force when the
     * meaning of every index changes underneath them; holding a different
     * order turns all of RAM into Device and the register window into
     * cacheable memory at that moment.
     */
    mov x8, #0xff04
    movk x8, #0x4400, lsl #16
    msr mair_el2, x8
    mov x8, #0x0019             /* 39-bit VA, level 1 start, walks uncached */
    movk x8, #0x8080, lsl #16   /* TG1 must hold a defined grain even unused */
    /* The size this core implements, capped at what the field can hold. */
    mrs x9, id_aa64mmfr0_el1
    and x9, x9, #0xf            /* PARange */
    mov x10, #5
    cmp x9, x10
    csel x9, x9, x10, lo
    orr x8, x8, x9, lsl #16     /* PS, where the non-E2H layout keeps it */
    lsl x9, x9, #32
    orr x8, x8, x9              /* IPS, where the E2H layout keeps it */
    msr tcr_el2, x8
    msr ttbr0_el2, x5
    dsb sy
    tlbi alle2
    dsb sy
    isb
    /* Record what the translation registers actually read back as. */
    mov x10, #0xff80
    movk x10, #0x17, lsl #16
    mrs x11, hcr_el2
    str x11, [x10]
    mrs x11, tcr_el2
    str x11, [x10, #8]
    mrs x11, ttbr0_el2
    str x11, [x10, #16]
    ldr x11, [x5]               /* level-1 descriptor for the machine map */
    str x11, [x10, #24]
    mrs x11, id_aa64mmfr0_el1
    str x11, [x10, #32]

    /* iBSS selects the 16 KiB TCR before enabling this inherited table. */
    ic iallu
    dsb sy
    isb

    /* The direct branch enters with everything but the arguments cleared, so
     * match it and do not leak the scratch used here. */
    mov x4, xzr
    mov x5, xzr
    mov x6, xzr
    mov x7, xzr
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mrs x12, sctlr_el2
    bic x12, x12, #1           /* M */
    msr sctlr_el2, x12
    isb
    mov x12, xzr
    isb
    eret
    .p2align 7
.Lel2_low64_irq:
    mov x12, #9
    b .Lel2_record
    .p2align 7
.Lel2_low64_fiq:
    mov x12, #10
    b .Lel2_record
    .p2align 7
.Lel2_low64_serr:
    mov x12, #11
    b .Lel2_record
    /* Lower EL, AArch32. */
    .p2align 7
.Lel2_low32_sync:
    mov x12, #12
    b .Lel2_record
    .p2align 7
.Lel2_low32_irq:
    mov x12, #13
    b .Lel2_record
    .p2align 7
.Lel2_low32_fiq:
    mov x12, #14
    b .Lel2_record
    .p2align 7
.Lel2_low32_serr:
    mov x12, #15
    b .Lel2_record
.size avp_el2_vector_table, .-avp_el2_vector_table

/*
 * void runtime_boot_enter_payload_at_el2(image_base, image_size, image_type,
 *                                        context, entry)
 *
 * The first four arguments are the payload's, in the registers it expects;
 * x4 carries where to go. Does not return.
 */
.p2align 2
.global runtime_boot_enter_payload_at_el2
.type runtime_boot_enter_payload_at_el2,@function
runtime_boot_enter_payload_at_el2:
    hvc #0
.Lel2_enter_stuck:
    b .Lel2_enter_stuck
.size runtime_boot_enter_payload_at_el2, .-runtime_boot_enter_payload_at_el2

/*
 * Record an unexpected EL2 exception into the ROM aperture and park. The MMU
 * is off at EL2 here, so these are plain physical stores.
 */
.Lel2_record:
    mov x10, #0xff00
    movk x10, #0x17, lsl #16
    mrs x11, esr_el2
    str x11, [x10]
    mrs x11, elr_el2
    str x11, [x10, #8]
    mrs x11, far_el2
    str x11, [x10, #16]
    str x12, [x10, #24]
.Lel2_parked:
    b .Lel2_parked

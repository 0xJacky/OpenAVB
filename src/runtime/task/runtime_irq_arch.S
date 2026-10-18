.section .text.runtime_irq,"ax",@progbits
.p2align 2
.global runtime_irq_arch_controller_initialize
.type runtime_irq_arch_controller_initialize,@function
runtime_irq_arch_controller_initialize:
    mrs x0, icc_sre_el1
    orr x0, x0, #1
    msr icc_sre_el1, x0
    isb
    msr icc_bpr0_el1, xzr
    msr icc_bpr1_el1, xzr
    mov x0, #0xff
    msr icc_pmr_el1, x0
    mrs x0, icc_ctlr_el1
    bic x0, x0, #1
    msr icc_ctlr_el1, x0
    mov x0, #1
    msr icc_igrpen0_el1, x0
    msr icc_igrpen1_el1, x0
    isb
    ret
.size runtime_irq_arch_controller_initialize, \
    .-runtime_irq_arch_controller_initialize

.p2align 2
.global runtime_irq_arch_virtual_timer_initialize
.type runtime_irq_arch_virtual_timer_initialize,@function
runtime_irq_arch_virtual_timer_initialize:
    movz w8, #0x14
    movk w8, #0x1001, lsl #16
    mov w9, #0x10000000
    mov w10, #3
    str w10, [x9]
    ldr w9, [x8]
    tbz w9, #2, 1f
    and w9, w9, #0xfffffff9
    str w9, [x8]
0:
    ldr w9, [x8]
    tbnz w9, #2, 0b
1:
    movz w8, #0x80
    movk w8, #0x1002, lsl #16
    mov w9, #0xf7ffffff
    str w9, [x8]
    ret
.size runtime_irq_arch_virtual_timer_initialize, \
    .-runtime_irq_arch_virtual_timer_initialize

.p2align 2
.global runtime_irq_arch_acknowledge_group0
.type runtime_irq_arch_acknowledge_group0,@function
runtime_irq_arch_acknowledge_group0:
    mrs x0, icc_iar0_el1
    dmb sy
    ret
.size runtime_irq_arch_acknowledge_group0, \
    .-runtime_irq_arch_acknowledge_group0

.p2align 2
.global runtime_irq_arch_end_group0
.type runtime_irq_arch_end_group0,@function
runtime_irq_arch_end_group0:
    msr icc_eoir0_el1, x0
    ret
.size runtime_irq_arch_end_group0, .-runtime_irq_arch_end_group0

.p2align 2
.global runtime_irq_arch_set_virtual_timer_enabled
.type runtime_irq_arch_set_virtual_timer_enabled,@function
runtime_irq_arch_set_virtual_timer_enabled:
    movz w8, #0x100
    movk w8, #0x1002, lsl #16
    mov w9, #0x08000000
    cbz w0, 0f
    str w9, [x8]
    ret
0:
    add x8, x8, #0x80
    str w9, [x8]
    ret
.size runtime_irq_arch_set_virtual_timer_enabled, \
    .-runtime_irq_arch_set_virtual_timer_enabled

.p2align 2
.global runtime_irq_arch_mask
.type runtime_irq_arch_mask,@function
runtime_irq_arch_mask:
    mrs x0, daif
    msr daifset, #3
    ret
.size runtime_irq_arch_mask, .-runtime_irq_arch_mask

.p2align 2
.global runtime_irq_arch_restore
.type runtime_irq_arch_restore,@function
runtime_irq_arch_restore:
    msr daif, x0
    ret
.size runtime_irq_arch_restore, .-runtime_irq_arch_restore

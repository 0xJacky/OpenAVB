.section .text.runtime_timer,"ax",@progbits
.p2align 2
.global runtime_timer_arch_initialize
.type runtime_timer_arch_initialize,@function
runtime_timer_arch_initialize:
    mov w0, #2
    msr cntv_ctl_el0, x0
    isb
    ret
.size runtime_timer_arch_initialize, .-runtime_timer_arch_initialize

.p2align 2
.global runtime_timer_arch_counter
.type runtime_timer_arch_counter,@function
runtime_timer_arch_counter:
    dsb sy
    isb
    mrs x0, cntvct_el0
    ret
.size runtime_timer_arch_counter, .-runtime_timer_arch_counter

.p2align 2
.global runtime_timer_arch_frequency
.type runtime_timer_arch_frequency,@function
runtime_timer_arch_frequency:
    mrs x0, cntfrq_el0
    ret
.size runtime_timer_arch_frequency, .-runtime_timer_arch_frequency

.p2align 2
.global runtime_timer_arch_program
.type runtime_timer_arch_program,@function
runtime_timer_arch_program:
    mov w1, #0x7fffffff
    msr cntv_tval_el0, x1
    isb
    mov w1, #1
    msr cntv_ctl_el0, x1
    isb
    msr cntv_tval_el0, x0
    isb
    ret
.size runtime_timer_arch_program, .-runtime_timer_arch_program

.p2align 2
.global runtime_timer_arch_mask
.type runtime_timer_arch_mask,@function
runtime_timer_arch_mask:
    mov w0, #2
    msr cntv_ctl_el0, x0
    isb
    ldr x1, =0x10020280
    mov w0, #0x08000000
    str w0, [x1]
    ret
.size runtime_timer_arch_mask, .-runtime_timer_arch_mask

.p2align 2
.global runtime_timer_arch_interrupt_pending
.type runtime_timer_arch_interrupt_pending,@function
runtime_timer_arch_interrupt_pending:
    mrs x0, isr_el1
    ubfx x0, x0, #6, #1
    ret
.size runtime_timer_arch_interrupt_pending, .-runtime_timer_arch_interrupt_pending

.p2align 2
.global runtime_timer_arch_irq_pending
.type runtime_timer_arch_irq_pending,@function
runtime_timer_arch_irq_pending:
    mrs x0, isr_el1
    ubfx x0, x0, #7, #1
    ret
.size runtime_timer_arch_irq_pending, .-runtime_timer_arch_irq_pending

.p2align 2
.global runtime_timer_arch_wait
.type runtime_timer_arch_wait,@function
runtime_timer_arch_wait:
    dsb sy
    isb
    wfi
    ret
.size runtime_timer_arch_wait, .-runtime_timer_arch_wait

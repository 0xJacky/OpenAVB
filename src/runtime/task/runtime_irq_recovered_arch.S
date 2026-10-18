.section .text.runtime_irq_recovered,"axR",@progbits
.p2align 2

.global runtime_virtual_timer_control_read
.type runtime_virtual_timer_control_read, %function
runtime_virtual_timer_control_read:
    isb
    mrs x0, cntv_ctl_el0
    ret
.size runtime_virtual_timer_control_read, .-runtime_virtual_timer_control_read

.global runtime_virtual_timer_control_write
.type runtime_virtual_timer_control_write, %function
runtime_virtual_timer_control_write:
    msr cntv_ctl_el0, x0
    isb
    ret
.size runtime_virtual_timer_control_write, .-runtime_virtual_timer_control_write

.global runtime_virtual_timer_tval_write
.type runtime_virtual_timer_tval_write, %function
runtime_virtual_timer_tval_write:
    msr cntv_tval_el0, x0
    isb
    ret
.size runtime_virtual_timer_tval_write, .-runtime_virtual_timer_tval_write

.global runtime_wait_for_interrupt_once
.type runtime_wait_for_interrupt_once, %function
runtime_wait_for_interrupt_once:
    dsb sy
    isb
    wfi
    ret
.size runtime_wait_for_interrupt_once, .-runtime_wait_for_interrupt_once

.global runtime_interrupt_mask_push_svc
.type runtime_interrupt_mask_push_svc, %function
runtime_interrupt_mask_push_svc:
    svc #0x22
    ret
.size runtime_interrupt_mask_push_svc, .-runtime_interrupt_mask_push_svc

.global runtime_interrupt_mask_pop_svc
.type runtime_interrupt_mask_pop_svc, %function
runtime_interrupt_mask_pop_svc:
    svc #0x23
    ret
.size runtime_interrupt_mask_pop_svc, .-runtime_interrupt_mask_pop_svc

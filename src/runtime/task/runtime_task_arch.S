.section .text.runtime_task,"ax",@progbits
.arch_extension pauth
.p2align 2
.p2align 2
.global runtime_task_arch_set_current
.type runtime_task_arch_set_current,@function
runtime_task_arch_set_current:
    msr tpidr_el1, x0
    ret
.size runtime_task_arch_set_current, .-runtime_task_arch_set_current

.p2align 2
.global runtime_task_arch_initialize_frame
.type runtime_task_arch_initialize_frame,@function
runtime_task_arch_initialize_frame:
    mrs x1, fpsr
    str x1, [x0, #0x110]
    mrs x1, fpcr
    str x1, [x0, #0x118]
    mrs x1, tpidr_el0
    str x1, [x0, #0x120]
    mrs x1, tpidrro_el0
    str x1, [x0, #0x128]
    mrs x1, ttbr1_el1
    str x1, [x0, #0x130]
    mrs x1, ttbr0_el1
    str x1, [x0, #0x360]
    adrp x1, __stack_chk_guard
    add x1, x1, :lo12:__stack_chk_guard
    ldr x1, [x1]
    str x1, [x0, #0x340]
    ret
.size runtime_task_arch_initialize_frame, .-runtime_task_arch_initialize_frame

.p2align 2
.global runtime_task_arch_generate_guard_seed
.type runtime_task_arch_generate_guard_seed,@function
runtime_task_arch_generate_guard_seed:
1:
    mrs x0, cntvct_el0
    eor x0, x0, x0, lsl #13
    eor x0, x0, x0, lsr #7
    eor x0, x0, x0, lsl #17
    cbz x0, 1b
    ret
.size runtime_task_arch_generate_guard_seed, .-runtime_task_arch_generate_guard_seed

.p2align 2
.global runtime_task_arch_sign_exception_return
.type runtime_task_arch_sign_exception_return,@function
runtime_task_arch_sign_exception_return:
    paciza x0
    ret
.size runtime_task_arch_sign_exception_return, .-runtime_task_arch_sign_exception_return

.p2align 2
.global runtime_idle_pump
.type runtime_idle_pump,@function
runtime_idle_pump:
    svc #0x20
    ret
.size runtime_idle_pump, .-runtime_idle_pump

.p2align 2
.global runtime_task_yield
.type runtime_task_yield,@function
runtime_task_yield:
    svc #0x42
    ret
.size runtime_task_yield, .-runtime_task_yield

.p2align 2
.global runtime_task_wait_until
.type runtime_task_wait_until,@function
runtime_task_wait_until:
    svc #0x43
    ret
.size runtime_task_wait_until, .-runtime_task_wait_until

.p2align 2
.global runtime_task_mask_interrupts
.type runtime_task_mask_interrupts,@function
runtime_task_mask_interrupts:
    svc #0x22
    ret
.size runtime_task_mask_interrupts, .-runtime_task_mask_interrupts

.p2align 2
.global runtime_task_unmask_interrupts
.type runtime_task_unmask_interrupts,@function
runtime_task_unmask_interrupts:
    svc #0x23
    ret
.size runtime_task_unmask_interrupts, .-runtime_task_unmask_interrupts

.p2align 2
.global runtime_task_activate_capability
.type runtime_task_activate_capability,@function
runtime_task_activate_capability:
    svc #0x41
    ret
.size runtime_task_activate_capability, .-runtime_task_activate_capability

.section .text.runtime_initialize_bootstrap_stack_guard,"ax",@progbits
.p2align 2
.global runtime_initialize_bootstrap_stack_guard
.type runtime_initialize_bootstrap_stack_guard,@function
runtime_initialize_bootstrap_stack_guard:
    movz x0, 0x746b
    movk x0, 0x0035, lsl #16
    movk x0, 0x6372, lsl #32
    movk x0, 0x4d69, lsl #48
    adrp x1, runtime_staged_stack_guard
    str x0, [x1, :lo12:runtime_staged_stack_guard]
    adrp x1, __stack_chk_guard
    str x0, [x1, :lo12:__stack_chk_guard]
    mov x0, -1
    msr tpidrro_el0, x0
    ret
.size runtime_initialize_bootstrap_stack_guard, .-runtime_initialize_bootstrap_stack_guard

.section .text.runtime_initialize_read_only_thread_pointer,"ax",@progbits
.p2align 2
.global runtime_initialize_read_only_thread_pointer
.type runtime_initialize_read_only_thread_pointer,@function
runtime_initialize_read_only_thread_pointer:
    mov x0, -1
    msr tpidrro_el0, x0
    ret
.size runtime_initialize_read_only_thread_pointer, .-runtime_initialize_read_only_thread_pointer

.section .text.runtime_compiler_outliners_recovered_arch,"axR",@progbits
.p2align 2

/* These leaves are stack-layout ABI, so C wrappers would change semantics. */

.global j_runtime_data_memory_barrier_system
.type j_runtime_data_memory_barrier_system,@function
j_runtime_data_memory_barrier_system:
    dmb sy
    ret
.size j_runtime_data_memory_barrier_system, .-j_runtime_data_memory_barrier_system

.global runtime_outlined_frame90_restore_x19_x28
.type runtime_outlined_frame90_restore_x19_x28,@function
runtime_outlined_frame90_restore_x19_x28:
    ldp x20, x19, [sp, #0x70]
    ldp x22, x21, [sp, #0x60]
    ldp x24, x23, [sp, #0x50]
    ldp x26, x25, [sp, #0x40]
    ldp x28, x27, [sp, #0x30]
    add sp, sp, #0x90
    ret
.size runtime_outlined_frame90_restore_x19_x28, .-runtime_outlined_frame90_restore_x19_x28

.global runtime_outlined_frame90_save_x19_x28
.type runtime_outlined_frame90_save_x19_x28,@function
runtime_outlined_frame90_save_x19_x28:
    sub sp, sp, #0x90
    stp x28, x27, [sp, #0x30]
    stp x26, x25, [sp, #0x40]
    stp x24, x23, [sp, #0x50]
    stp x22, x21, [sp, #0x60]
    stp x20, x19, [sp, #0x70]
    ret
.size runtime_outlined_frame90_save_x19_x28, .-runtime_outlined_frame90_save_x19_x28

.global runtime_outlined_frame_c0_save_x19_x28
.type runtime_outlined_frame_c0_save_x19_x28,@function
runtime_outlined_frame_c0_save_x19_x28:
    sub sp, sp, #0xc0
    stp x28, x27, [sp, #0x60]
    stp x26, x25, [sp, #0x70]
    stp x24, x23, [sp, #0x80]
    stp x22, x21, [sp, #0x90]
    stp x20, x19, [sp, #0xa0]
    ret
.size runtime_outlined_frame_c0_save_x19_x28, .-runtime_outlined_frame_c0_save_x19_x28

.global runtime_outlined_frame70_restore_x19_x28
.type runtime_outlined_frame70_restore_x19_x28,@function
runtime_outlined_frame70_restore_x19_x28:
    ldp x20, x19, [sp, #0x50]
    ldp x22, x21, [sp, #0x40]
    ldp x24, x23, [sp, #0x30]
    ldp x26, x25, [sp, #0x20]
    ldp x28, x27, [sp, #0x10]
    add sp, sp, #0x70
    ret
.size runtime_outlined_frame70_restore_x19_x28, .-runtime_outlined_frame70_restore_x19_x28

.global runtime_outlined_frame_a0_restore_x19_x28
.type runtime_outlined_frame_a0_restore_x19_x28,@function
runtime_outlined_frame_a0_restore_x19_x28:
    ldp x20, x19, [sp, #0x80]
    ldp x22, x21, [sp, #0x70]
    ldp x24, x23, [sp, #0x60]
    ldp x26, x25, [sp, #0x50]
    ldp x28, x27, [sp, #0x40]
    add sp, sp, #0xa0
    ret
.size runtime_outlined_frame_a0_restore_x19_x28, .-runtime_outlined_frame_a0_restore_x19_x28

.global runtime_outlined_frame70_save_x19_x28
.type runtime_outlined_frame70_save_x19_x28,@function
runtime_outlined_frame70_save_x19_x28:
    sub sp, sp, #0x70
    stp x28, x27, [sp, #0x10]
    stp x26, x25, [sp, #0x20]
    stp x24, x23, [sp, #0x30]
    stp x22, x21, [sp, #0x40]
    stp x20, x19, [sp, #0x50]
    ret
.size runtime_outlined_frame70_save_x19_x28, .-runtime_outlined_frame70_save_x19_x28

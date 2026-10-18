.section .text.runtime_exception_frame_recovered_arch, "ax", @progbits
.p2align 2

.global runtime_exception_capture_system_registers_arch
.type runtime_exception_capture_system_registers_arch, %function
runtime_exception_capture_system_registers_arch:
    mrs x1, sp_el0
    str x1, [x0, #0x00]
    mrs x1, spsr_el1
    str x1, [x0, #0x08]
    mrs x1, fpsr
    str x1, [x0, #0x10]
    mrs x1, fpcr
    str x1, [x0, #0x18]
    mrs x1, tpidr_el0
    str x1, [x0, #0x20]
    mrs x1, ttbr0_el1
    str x1, [x0, #0x28]
    mrs x1, ttbr1_el1
    str x1, [x0, #0x30]
    stp q0, q1, [x0, #0x40]
    stp q2, q3, [x0, #0x60]
    stp q4, q5, [x0, #0x80]
    stp q6, q7, [x0, #0xa0]
    stp q8, q9, [x0, #0xc0]
    stp q10, q11, [x0, #0xe0]
    stp q12, q13, [x0, #0x100]
    stp q14, q15, [x0, #0x120]
    stp q16, q17, [x0, #0x140]
    stp q18, q19, [x0, #0x160]
    stp q20, q21, [x0, #0x180]
    stp q22, q23, [x0, #0x1a0]
    stp q24, q25, [x0, #0x1c0]
    stp q26, q27, [x0, #0x1e0]
    stp q28, q29, [x0, #0x200]
    stp q30, q31, [x0, #0x220]
    ret
.size runtime_exception_capture_system_registers_arch, .-runtime_exception_capture_system_registers_arch

.section .note.GNU-stack, "", @progbits

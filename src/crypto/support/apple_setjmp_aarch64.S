.section .text.avp_setjmp,"ax",@progbits
.global avp_setjmp
.type avp_setjmp, %function
avp_setjmp:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x9, sp
    str x9, [x0, #96]
    mov w0, wzr
    ret
.size avp_setjmp, . - avp_setjmp

.section .text.avp_longjmp,"ax",@progbits
.global avp_longjmp
.type avp_longjmp, %function
avp_longjmp:
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldr x9, [x0, #96]
    ldp x29, x30, [x0, #80]
    mov sp, x9
    mov w0, w1
    cbnz w0, 1f
    mov w0, #1
1:
    ret
.size avp_longjmp, . - avp_longjmp

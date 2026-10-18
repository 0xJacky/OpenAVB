.section .text.handoff,"ax",@progbits
.p2align 2
.global source_image_handoff
.type source_image_handoff,@function
source_image_handoff:
    mov x5, x3
    add x3, x0, x1
    and x4, x0, #-64
1:
    dc cvau, x4
    add x4, x4, #64
    cmp x4, x3
    b.lo 1b
    dsb ish
    and x4, x0, #-64
2:
    ic ivau, x4
    add x4, x4, #64
    cmp x4, x3
    b.lo 2b
    dsb ish
    isb
    /* Apple's terminal handoff service receives the payload entry, a null
     * context, the selected boot mode, and the resident-trampoline request. */
    mov x0, x2
    mov x1, xzr
    mov x2, x5
    mov w3, #1
    svc #3
    b pvpanic_trigger
.size source_image_handoff, .-source_image_handoff

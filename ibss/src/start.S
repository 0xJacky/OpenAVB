.section .source_image_header,"a",@progbits
.p2align 3
.long 0x53424953
.short 1
.short 32
.long __source_image_end - 0x7006c000
.long _source_ibss_start - 0x7006c000
.quad 0x5352434942535331
.long 0
.long 0

.section .text.start,"ax",@progbits
.p2align 4
.global _source_ibss_start
.type _source_ibss_start,@function
_source_ibss_start:
    ldr x0, =__stack_top
    mov sp, x0
    adr x1, source_ibss_vector_table
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
    bl source_ibss_main
5:
    wfe
    b 5b
.size _source_ibss_start, .-_source_ibss_start

.p2align 11
.global source_ibss_vector_table
source_ibss_vector_table:
.rept 16
    b pvpanic_trigger
    .rept 31
        nop
    .endr
.endr

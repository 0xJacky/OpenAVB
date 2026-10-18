.section .text.runtime_firebloom_metadata,"ax",@progbits
.p2align 2
.global runtime_firebloom_restore_primary_tuple
.type runtime_firebloom_restore_primary_tuple,@function
runtime_firebloom_restore_primary_tuple:
    mov x0, x22
    mov x1, x21
    mov x2, x20
    mov x3, x19
    ret
.size runtime_firebloom_restore_primary_tuple, \
    .-runtime_firebloom_restore_primary_tuple

.p2align 2
.global runtime_firebloom_restore_secondary_tuple
.type runtime_firebloom_restore_secondary_tuple,@function
runtime_firebloom_restore_secondary_tuple:
    mov x4, x22
    mov x5, x21
    mov x6, x20
    mov x7, x19
    ret
.size runtime_firebloom_restore_secondary_tuple, \
    .-runtime_firebloom_restore_secondary_tuple

.p2align 2
.global runtime_firebloom_save_primary_tuple
.type runtime_firebloom_save_primary_tuple,@function
runtime_firebloom_save_primary_tuple:
    mov x19, x3
    mov x20, x2
    mov x21, x1
    mov x22, x0
    ret
.size runtime_firebloom_save_primary_tuple, \
    .-runtime_firebloom_save_primary_tuple

.p2align 2
.global runtime_firebloom_promote_secondary_tuple
.type runtime_firebloom_promote_secondary_tuple,@function
runtime_firebloom_promote_secondary_tuple:
    mov x0, x4
    mov x1, x5
    mov x2, x6
    mov x3, x7
    ret
.size runtime_firebloom_promote_secondary_tuple, \
    .-runtime_firebloom_promote_secondary_tuple

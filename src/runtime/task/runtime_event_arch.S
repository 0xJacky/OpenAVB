.section .text.runtime_event,"ax",@progbits
.p2align 2
.global runtime_event_create
.type runtime_event_create,@function
runtime_event_create:
    mov w2, #3
    svc #0x50
    ret
.size runtime_event_create, .-runtime_event_create

.p2align 2
.global runtime_event_wait
.type runtime_event_wait,@function
runtime_event_wait:
    svc #0x51
    ret
.size runtime_event_wait, .-runtime_event_wait

.p2align 2
.global runtime_event_set
.type runtime_event_set,@function
runtime_event_set:
    mov x1, #1
    svc #0x52
    ret
.size runtime_event_set, .-runtime_event_set

.p2align 2
.global runtime_event_clear
.type runtime_event_clear,@function
runtime_event_clear:
    mov x1, #0
    svc #0x52
    ret
.size runtime_event_clear, .-runtime_event_clear

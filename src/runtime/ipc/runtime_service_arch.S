.section .text.runtime_service,"ax",@progbits
.p2align 2
.global runtime_service_call
.type runtime_service_call,@function
runtime_service_call:
    svc #0xc2
    ret
.size runtime_service_call, .-runtime_service_call

.p2align 2
.global runtime_service_receive
.type runtime_service_receive,@function
runtime_service_receive:
    svc #0xc3
    ret
.size runtime_service_receive, .-runtime_service_receive

.p2align 2
.global runtime_service_complete
.type runtime_service_complete,@function
runtime_service_complete:
    svc #0xc4
    ret
.size runtime_service_complete, .-runtime_service_complete

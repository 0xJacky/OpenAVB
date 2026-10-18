.section .text.runtime_task_lifecycle,"ax",@progbits
.p2align 2
.global runtime_task_create
.type runtime_task_create,@function
runtime_task_create:
    svc #0x40
    ret
.size runtime_task_create, .-runtime_task_create

.p2align 2
.global runtime_task_exit
.type runtime_task_exit,@function
runtime_task_exit:
    sxtw x0, w0
    svc #0x44
    brk #0
.size runtime_task_exit, .-runtime_task_exit

.p2align 2
.global runtime_task_join
.type runtime_task_join,@function
runtime_task_join:
    and x0, x0, #0xffff
    svc #0x45
    ret
.size runtime_task_join, .-runtime_task_join

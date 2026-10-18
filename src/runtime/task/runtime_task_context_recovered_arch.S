.section .text.runtime_task_context_recovered_arch, "ax", @progbits
.p2align 2

.global runtime_task_context_read_current_arch
.type runtime_task_context_read_current_arch, %function
runtime_task_context_read_current_arch:
    mrs x0, tpidrro_el0
    ret
.size runtime_task_context_read_current_arch, .-runtime_task_context_read_current_arch

.p2align 2
.global runtime_task_context_read_selected_arch
.type runtime_task_context_read_selected_arch, %function
runtime_task_context_read_selected_arch:
    mrs x0, tpidr_el1
    ret
.size runtime_task_context_read_selected_arch, .-runtime_task_context_read_selected_arch

.section .note.GNU-stack, "", @progbits

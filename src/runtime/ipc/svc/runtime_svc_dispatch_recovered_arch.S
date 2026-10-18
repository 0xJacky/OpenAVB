.section .text.runtime_svc_dispatch_recovered_arch,"axR",@progbits
.p2align 2
.global runtime_hypervisor_call_zero
.type runtime_hypervisor_call_zero,@function
runtime_hypervisor_call_zero:
    hvc #0
    ret
.size runtime_hypervisor_call_zero, .-runtime_hypervisor_call_zero

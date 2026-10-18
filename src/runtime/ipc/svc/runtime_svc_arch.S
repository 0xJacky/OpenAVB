.section .text.runtime_svc,"ax",@progbits
.p2align 2
.global runtime_domain_query_locality
.type runtime_domain_query_locality,@function
runtime_domain_query_locality:
    svc #0xc5
    ret
.size runtime_domain_query_locality, .-runtime_domain_query_locality

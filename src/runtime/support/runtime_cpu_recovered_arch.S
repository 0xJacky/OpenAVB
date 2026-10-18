.section .text.runtime_cpu_recovered,"axR",@progbits
.p2align 2

.macro runtime_cpu_write_key name, low_register, high_register
.global \name
.type \name, %function
\name:
    msr \low_register, x1
    msr \high_register, x0
    dsb sy
    isb
    ret
.size \name, .-\name
.endm

runtime_cpu_write_key runtime_write_apiakey_el1, s3_0_c2_c1_0, s3_0_c2_c1_1
runtime_cpu_write_key runtime_write_apibkey_el1, s3_0_c2_c1_2, s3_0_c2_c1_3
runtime_cpu_write_key runtime_write_apdakey_el1, s3_0_c2_c2_0, s3_0_c2_c2_1
runtime_cpu_write_key runtime_write_apdbkey_el1, s3_0_c2_c2_2, s3_0_c2_c2_3
runtime_cpu_write_key runtime_write_apgakey_el1, s3_0_c2_c3_0, s3_0_c2_c3_1

.section .text.runtime_unprivileged_memory_recovered_arch,"ax",@progbits
.p2align 2

.global runtime_unprivileged_load_u64_arch
.type runtime_unprivileged_load_u64_arch,@function
runtime_unprivileged_load_u64_arch:
    ldtr x0, [x0]
    ret
.size runtime_unprivileged_load_u64_arch, .-runtime_unprivileged_load_u64_arch

.global runtime_unprivileged_load_u32_arch
.type runtime_unprivileged_load_u32_arch,@function
runtime_unprivileged_load_u32_arch:
    ldtr w0, [x0]
    ret
.size runtime_unprivileged_load_u32_arch, .-runtime_unprivileged_load_u32_arch

.global runtime_unprivileged_load_u16_arch
.type runtime_unprivileged_load_u16_arch,@function
runtime_unprivileged_load_u16_arch:
    ldtrh w0, [x0]
    ret
.size runtime_unprivileged_load_u16_arch, .-runtime_unprivileged_load_u16_arch

.global runtime_unprivileged_load_u8_arch
.type runtime_unprivileged_load_u8_arch,@function
runtime_unprivileged_load_u8_arch:
    ldtrb w0, [x0]
    ret
.size runtime_unprivileged_load_u8_arch, .-runtime_unprivileged_load_u8_arch

.global runtime_unprivileged_store_u64_arch
.type runtime_unprivileged_store_u64_arch,@function
runtime_unprivileged_store_u64_arch:
    sttr x1, [x0]
    ret
.size runtime_unprivileged_store_u64_arch, .-runtime_unprivileged_store_u64_arch

.global runtime_unprivileged_store_u32_arch
.type runtime_unprivileged_store_u32_arch,@function
runtime_unprivileged_store_u32_arch:
    sttr w1, [x0]
    ret
.size runtime_unprivileged_store_u32_arch, .-runtime_unprivileged_store_u32_arch

.global runtime_unprivileged_store_u16_arch
.type runtime_unprivileged_store_u16_arch,@function
runtime_unprivileged_store_u16_arch:
    sttrh w1, [x0]
    ret
.size runtime_unprivileged_store_u16_arch, .-runtime_unprivileged_store_u16_arch

.global runtime_unprivileged_store_u8_arch
.type runtime_unprivileged_store_u8_arch,@function
runtime_unprivileged_store_u8_arch:
    sttrb w1, [x0]
    ret
.size runtime_unprivileged_store_u8_arch, .-runtime_unprivileged_store_u8_arch

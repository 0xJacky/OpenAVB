.section .text.runtime_page_table,"ax",@progbits
.p2align 2

.global runtime_page_table_arch_publish
.type runtime_page_table_arch_publish,@function
runtime_page_table_arch_publish:
    // Page-table writes must reach the shareable domain before a walk begins.
    dsb ishst
    isb
    ret
.size runtime_page_table_arch_publish, .-runtime_page_table_arch_publish

.p2align 2
.global runtime_page_table_arch_invalidate_all
.type runtime_page_table_arch_invalidate_all,@function
runtime_page_table_arch_invalidate_all:
    // The runtime uses the architected full EL1 invalidation order.
    dsb sy
    tlbi vmalle1
    dsb sy
    isb
    ret
.size runtime_page_table_arch_invalidate_all, .-runtime_page_table_arch_invalidate_all

.p2align 2
.global runtime_page_table_arch_invalidate_asid
.type runtime_page_table_arch_invalidate_asid,@function
runtime_page_table_arch_invalidate_asid:
    lsl x0, x0, #48
    dsb sy
    tlbi aside1, x0
    dsb sy
    isb
    ret
.size runtime_page_table_arch_invalidate_asid, .-runtime_page_table_arch_invalidate_asid

.p2align 2
.global runtime_page_table_arch_read_ttbr0_el1
.type runtime_page_table_arch_read_ttbr0_el1,@function
runtime_page_table_arch_read_ttbr0_el1:
    mrs x0, ttbr0_el1
    ret
.size runtime_page_table_arch_read_ttbr0_el1, .-runtime_page_table_arch_read_ttbr0_el1

.p2align 2
.global runtime_page_table_arch_read_ttbr1_el1
.type runtime_page_table_arch_read_ttbr1_el1,@function
runtime_page_table_arch_read_ttbr1_el1:
    mrs x0, ttbr1_el1
    ret
.size runtime_page_table_arch_read_ttbr1_el1, .-runtime_page_table_arch_read_ttbr1_el1

.p2align 2
.global runtime_page_table_arch_write_ttbr0_el1
.type runtime_page_table_arch_write_ttbr0_el1,@function
runtime_page_table_arch_write_ttbr0_el1:
    msr ttbr0_el1, x0
    isb
    ret
.size runtime_page_table_arch_write_ttbr0_el1, .-runtime_page_table_arch_write_ttbr0_el1

.p2align 2
.global runtime_page_table_arch_write_ttbr1_el1
.type runtime_page_table_arch_write_ttbr1_el1,@function
runtime_page_table_arch_write_ttbr1_el1:
    msr ttbr1_el1, x0
    isb
    ret
.size runtime_page_table_arch_write_ttbr1_el1, .-runtime_page_table_arch_write_ttbr1_el1

.p2align 2
.global runtime_page_table_arch_read_sctlr_el1
.type runtime_page_table_arch_read_sctlr_el1,@function
runtime_page_table_arch_read_sctlr_el1:
    mrs x0, sctlr_el1
    ret
.size runtime_page_table_arch_read_sctlr_el1, .-runtime_page_table_arch_read_sctlr_el1

.p2align 2
.global runtime_page_table_arch_read_tcr_el1
.type runtime_page_table_arch_read_tcr_el1,@function
runtime_page_table_arch_read_tcr_el1:
    mrs x0, tcr_el1
    ret
.size runtime_page_table_arch_read_tcr_el1, .-runtime_page_table_arch_read_tcr_el1

.p2align 2
.global runtime_page_table_arch_read_mair_el1
.type runtime_page_table_arch_read_mair_el1,@function
runtime_page_table_arch_read_mair_el1:
    mrs x0, mair_el1
    ret
.size runtime_page_table_arch_read_mair_el1, .-runtime_page_table_arch_read_mair_el1

.p2align 2
.global runtime_page_table_arch_read_id_aa64mmfr0_el1
.type runtime_page_table_arch_read_id_aa64mmfr0_el1,@function
runtime_page_table_arch_read_id_aa64mmfr0_el1:
    mrs x0, id_aa64mmfr0_el1
    ret
.size runtime_page_table_arch_read_id_aa64mmfr0_el1, .-runtime_page_table_arch_read_id_aa64mmfr0_el1

.p2align 2
.global runtime_page_table_arch_write_sctlr_el1
.type runtime_page_table_arch_write_sctlr_el1,@function
runtime_page_table_arch_write_sctlr_el1:
    msr sctlr_el1, x0
    isb
    ret
.size runtime_page_table_arch_write_sctlr_el1, .-runtime_page_table_arch_write_sctlr_el1

.p2align 2
.global runtime_page_table_arch_write_tcr_el1
.type runtime_page_table_arch_write_tcr_el1,@function
runtime_page_table_arch_write_tcr_el1:
    msr tcr_el1, x0
    isb
    ret
.size runtime_page_table_arch_write_tcr_el1, .-runtime_page_table_arch_write_tcr_el1

.p2align 2
.global runtime_page_table_arch_write_mair_el1
.type runtime_page_table_arch_write_mair_el1,@function
runtime_page_table_arch_write_mair_el1:
    msr mair_el1, x0
    isb
    ret
.size runtime_page_table_arch_write_mair_el1, .-runtime_page_table_arch_write_mair_el1

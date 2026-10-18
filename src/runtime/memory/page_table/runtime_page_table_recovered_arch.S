.section .text.runtime_page_table_recovered_arch,"axR",@progbits
.p2align 2

.global runtime_instruction_cache_invalidate_all
.type runtime_instruction_cache_invalidate_all,@function
runtime_instruction_cache_invalidate_all:
    // Global instruction invalidation requires completion before execution resumes.
    ic iallu
    dsb sy
    isb
    ret
.size runtime_instruction_cache_invalidate_all, .-runtime_instruction_cache_invalidate_all

.p2align 2
.global runtime_branch_predictor_invalidate_all_noop
.type runtime_branch_predictor_invalidate_all_noop,@function
runtime_branch_predictor_invalidate_all_noop:
    ret
.size runtime_branch_predictor_invalidate_all_noop, .-runtime_branch_predictor_invalidate_all_noop

.p2align 2
.global runtime_tlb_invalidate_all_el1
.type runtime_tlb_invalidate_all_el1,@function
runtime_tlb_invalidate_all_el1:
    dsb sy
    tlbi vmalle1
    dsb sy
    isb
    ret
.size runtime_tlb_invalidate_all_el1, .-runtime_tlb_invalidate_all_el1

.p2align 2
.global runtime_tlb_invalidate_asid_el1
.type runtime_tlb_invalidate_asid_el1,@function
runtime_tlb_invalidate_asid_el1:
    dsb sy
    lsl x0, x0, #48
    tlbi aside1, x0
    dsb sy
    isb
    ret
.size runtime_tlb_invalidate_asid_el1, .-runtime_tlb_invalidate_asid_el1

.p2align 2
.global j_runtime_tlb_invalidate_asid_el1
.type j_runtime_tlb_invalidate_asid_el1,@function
j_runtime_tlb_invalidate_asid_el1:
    b runtime_tlb_invalidate_asid_el1
.size j_runtime_tlb_invalidate_asid_el1, .-j_runtime_tlb_invalidate_asid_el1

.p2align 2
.global runtime_write_tcr_el1
.type runtime_write_tcr_el1,@function
runtime_write_tcr_el1:
    msr tcr_el1, x0
    isb
    ret
.size runtime_write_tcr_el1, .-runtime_write_tcr_el1

.p2align 2
.global runtime_write_ttbr0_el1
.type runtime_write_ttbr0_el1,@function
runtime_write_ttbr0_el1:
    msr ttbr0_el1, x0
    isb
    ret
.size runtime_write_ttbr0_el1, .-runtime_write_ttbr0_el1

.p2align 2
.global runtime_write_ttbr1_el1
.type runtime_write_ttbr1_el1,@function
runtime_write_ttbr1_el1:
    msr ttbr1_el1, x0
    isb
    ret
.size runtime_write_ttbr1_el1, .-runtime_write_ttbr1_el1

.p2align 2
.global runtime_read_ttbr0_el1
.type runtime_read_ttbr0_el1,@function
runtime_read_ttbr0_el1:
    mrs x0, ttbr0_el1
    ret
.size runtime_read_ttbr0_el1, .-runtime_read_ttbr0_el1

.p2align 2
.global runtime_read_ttbr1_el1
.type runtime_read_ttbr1_el1,@function
runtime_read_ttbr1_el1:
    mrs x0, ttbr1_el1
    ret
.size runtime_read_ttbr1_el1, .-runtime_read_ttbr1_el1

.p2align 2
.global runtime_write_mair_el1
.type runtime_write_mair_el1,@function
runtime_write_mair_el1:
    msr mair_el1, x0
    isb
    ret
.size runtime_write_mair_el1, .-runtime_write_mair_el1

.p2align 2
.global runtime_tcr_el1_enable_ttbr1_walks
.type runtime_tcr_el1_enable_ttbr1_walks,@function
runtime_tcr_el1_enable_ttbr1_walks:
    mrs x0, tcr_el1
    bic x0, x0, #(1 << 23)
    b runtime_write_tcr_el1
.size runtime_tcr_el1_enable_ttbr1_walks, .-runtime_tcr_el1_enable_ttbr1_walks

.p2align 2
.global runtime_write_sctlr_el1_require_mmu_wxn
.type runtime_write_sctlr_el1_require_mmu_wxn,@function
runtime_write_sctlr_el1_require_mmu_wxn:
    // The contract publishes readiness only after M and WXN are active.
    orr x0, x0, #1
    orr x0, x0, #(1 << 19)
    msr sctlr_el1, x0
    dsb sy
    isb
    b runtime_page_table_recovered_publish_mmu_active
.size runtime_write_sctlr_el1_require_mmu_wxn, .-runtime_write_sctlr_el1_require_mmu_wxn

.p2align 2
.global runtime_disable_el0_timer_and_maintenance_access
.type runtime_disable_el0_timer_and_maintenance_access,@function
runtime_disable_el0_timer_and_maintenance_access:
    mrs x0, cntkctl_el1
    bic x0, x0, #1
    bic x0, x0, #2
    bic x0, x0, #(1 << 8)
    bic x0, x0, #(1 << 9)
    msr cntkctl_el1, x0
    mrs x0, sctlr_el1
    bic x0, x0, #(1 << 9)
    bic x0, x0, #(1 << 14)
    bic x0, x0, #(1 << 23)
    bic x0, x0, #(1 << 26)
    b runtime_write_sctlr_el1_require_mmu_wxn
.size runtime_disable_el0_timer_and_maintenance_access, .-runtime_disable_el0_timer_and_maintenance_access

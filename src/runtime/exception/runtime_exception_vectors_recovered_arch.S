.section .text.runtime_exception_vectors_recovered,"axR",@progbits
.p2align 7

.macro unsupported_vector name
.global \name
.type \name,%function
\name:
    b runtime_halt_forever
.size \name, .-\name
.p2align 7
.endm

.macro synchronous_vector name
.global \name
.type \name,%function
\name:
    b avp_exception_entry
.size \name, .-\name
.p2align 7
.endm

.macro timer_vector name
.global \name
.type \name,%function
\name:
    b avp_timer_fiq_entry
.size \name, .-\name
.p2align 7
.endm

.global runtime_halt_forever
.type runtime_halt_forever,%function
runtime_halt_forever:
    msr daifset, #0xf
1:
    wfi
    b 1b
.size runtime_halt_forever, .-runtime_halt_forever
.p2align 7

unsupported_vector runtime_vector_current_el_sp0_sync_unsupported /* runtime_vector_current_el_sp0_sync_unsupported: */
unsupported_vector runtime_vector_current_el_sp0_irq_unsupported /* runtime_vector_current_el_sp0_irq_unsupported: */
unsupported_vector runtime_vector_current_el_sp0_fiq_unsupported /* runtime_vector_current_el_sp0_fiq_unsupported: */
unsupported_vector runtime_vector_current_el_sp0_serror_unsupported /* runtime_vector_current_el_sp0_serror_unsupported: */
synchronous_vector runtime_vector_current_el_spx_sync /* runtime_vector_current_el_spx_sync: */
unsupported_vector runtime_vector_current_el_spx_irq /* runtime_vector_current_el_spx_irq: */
timer_vector runtime_vector_current_el_spx_fiq /* runtime_vector_current_el_spx_fiq: */
synchronous_vector runtime_vector_current_el_spx_serror /* runtime_vector_current_el_spx_serror: */
synchronous_vector runtime_vector_lower_el_aarch64_sync /* runtime_vector_lower_el_aarch64_sync: */
unsupported_vector runtime_vector_lower_el_aarch64_irq /* runtime_vector_lower_el_aarch64_irq: */
timer_vector runtime_vector_lower_el_aarch64_fiq /* runtime_vector_lower_el_aarch64_fiq: */
synchronous_vector runtime_vector_lower_el_aarch64_serror /* runtime_vector_lower_el_aarch64_serror: */
unsupported_vector runtime_vector_lower_el_aarch32_sync_unsupported /* runtime_vector_lower_el_aarch32_sync_unsupported: */
unsupported_vector runtime_vector_lower_el_aarch32_irq_unsupported /* runtime_vector_lower_el_aarch32_irq_unsupported: */
unsupported_vector runtime_vector_lower_el_aarch32_fiq_unsupported /* runtime_vector_lower_el_aarch32_fiq_unsupported: */
unsupported_vector runtime_vector_lower_el_aarch32_serror_unsupported /* runtime_vector_lower_el_aarch32_serror_unsupported: */

.section .note.GNU-stack,"",@progbits

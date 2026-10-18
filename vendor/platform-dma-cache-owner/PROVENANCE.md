# Platform DMA cache owner provenance

This module implements platform DMA cache maintenance: the operation
selector, 64-byte line alignment, the bootstrap/runtime whole-cache route,
interrupt-mask lifetime, and terminal validation behavior.

The clean-and-invalidate line operation delegates to the existing
`platform_dma_cacheline_clean_invalidate` implementation, which emits AArch64
`DC CIVAC`. Completion delegates to the existing runtime `DSB SY` barrier and
interrupt-mask entry points. The four platform no-op callbacks stay explicit
so their observable behavior and ELF symbols match the expected interface.

The adapter is original project code and imports no third-party implementation.
This provenance record documents its technical sources and makes no independent
license grant.

## Public specifications

- Arm A64 `DC CIVAC`: https://developer.arm.com/documentation/ddi0602/latest/Base-Instructions/DC-CIVAC--Data-or-unified-Cache-line-Clean-and-Invalidate-by-VA-to-PoC
- Arm memory ordering and `DSB`: https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/memory-access-ordering-part-3---memory-access-ordering-in-the-arm-architecture

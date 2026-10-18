# Runtime address space provenance

The address-space module delegates capability publication, audience checks,
physical-region validation, and 16 KiB page-table updates to OpenAVB's
existing runtime implementations.

Domain membership and mapping lifecycle were checked against Zephyr's memory
domain implementation at commit
`0d65ce46dda0626164503c50f37e6e1f59d310a9` (Apache-2.0), blob
`8fa4fcfa21cbb45af4e8c61a4a5ac7da85b9862a`.

No source text was copied. Platform-specific global-direct eligibility remains
an explicit policy table, while generic audience, subrange, bitmap, domain
window, shared-page, attach, detach, and unmap state machines are implemented
locally against the expected ROM interface.

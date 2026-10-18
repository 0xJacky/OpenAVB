# Runtime domain provenance

The implementation reuses OpenAVB's existing domain registry, capability
resolver, global runtime state, and Firebloom span copy operations. The public
design reference is Zephyr's memory-domain implementation at commit
`0d65ce46dda0626164503c50f37e6e1f59d310a9` (Apache-2.0):

- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/userspace/mem_domain.c
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The public contracts used here are validated domain membership, bounded domain
metadata, and explicit domain-to-thread or domain-to-capability association.
OpenAVB implements the AVP interface's 4-bit selector table, 16-bit public
handles, 0x118-byte domain records, and Firebloom bounded views. No Zephyr
source text is copied into this implementation.

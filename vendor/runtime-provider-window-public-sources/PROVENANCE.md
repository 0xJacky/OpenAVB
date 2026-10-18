# Runtime provider storage-window provenance

The implementation reuses OpenAVB's provider registry. Its public design
reference is Zephyr's disk-access layer at commit
`0d65ce46dda0626164503c50f37e6e1f59d310a9` (Apache-2.0):

- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/subsys/disk/disk_access.c
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/include/zephyr/storage/disk_access.h
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE
- https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9

The public contracts used here are registered storage operation tables,
sector-to-byte scaling, explicit backing contexts, and operation result
propagation. OpenAVB implements the AVP interface's 0x70-byte window,
0x60-byte backing descriptor, element-shift scaling, clipped subrange
forwarding, five callback slots, bounded tuples, and fixed source token. No
Zephyr source text is copied into this implementation.

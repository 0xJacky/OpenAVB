# Runtime typed-provider provenance

The implementation reuses OpenAVB's provider registry and follows the
registered operation-table model from Zephyr's disk-access layer at commit
`0d65ce46dda0626164503c50f37e6e1f59d310a9` (Apache-2.0):

- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/include/zephyr/storage/disk_access.h
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/subsys/disk/disk_access.c
- https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The reused contracts are explicit storage contexts, callback operation tables,
element-to-byte normalization, route result propagation, and registration
lifecycle. Descriptor/implementation separation, five callback slots, bounded
tuple validation, 16-byte route values, and ABI outliners are AVP-specific
integration code. No Zephyr source text is copied.

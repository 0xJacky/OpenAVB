# Runtime IRQ provenance

The implementation was reviewed against Zephyr's public interrupt API for
nested lock/unlock semantics. Timer system-register operations, SVC numbers,
the interrupt-state tuple, event codes, and runtime accounting follow the
AVP-specific ROM interface.

- Project: Zephyr
- Repository: https://github.com/zephyrproject-rtos/zephyr
- Commit: `0d65ce46dda0626164503c50f37e6e1f59d310a9`
- Tree: `db3fed4eddf0cb918cad68389bdf5ce64647c1f6`
- File: `include/zephyr/irq.h`
- File blob: `c8a2ed528afaa8407c75bf129353be0d716f10b9`
- License: Apache-2.0
- License blob: `8f71f43fee3f78649d238238cbde51e6d7055c82`
- Source URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/include/zephyr/irq.h
- License URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

No public source text is copied into this adapter.

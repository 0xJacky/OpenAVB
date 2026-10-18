# Runtime event provenance

The module uses the existing OpenAVB event and scheduler services. Its public
design reference is Zephyr's kernel event implementation at commit
`0d65ce46dda0626164503c50f37e6e1f59d310a9` (Apache-2.0):

- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/events.c
- https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The useful public contracts are syscall-visible event publication, bounded
waits, reset-on-consumption, and waking registered waiters. OpenAVB implements
the AVP interface's 16-bit capability ABI, absolute virtual-counter deadlines,
auto-reset/manual-reset state, and Firebloom-style bounded registration tuples.
No Zephyr source text is copied into the implementation.

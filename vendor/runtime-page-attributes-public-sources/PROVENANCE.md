# Runtime page-attribute provenance

The module implements the AVP interface's 16 KiB page geometry, ordered region
records, dual bitmap state, clear/set/mixed aggregation, and policy gates.
Its range mutation and classification delegate to OpenAVB's existing
`runtime_bounded_bitmap` module, which was reviewed against these public
implementations:

- FreeBSD `bitstring.h`, commit
  `3aac283613bd3fd0228a06d6c854ca0bf190ecfb`, BSD-3-Clause:
  https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
- Zephyr `bitarray.c`, commit
  `0d65ce46dda0626164503c50f37e6e1f59d310a9`, Apache-2.0:
  https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9

The registry layout, address policy, lookup contract, paired attribute modes,
and outliner entry points are AVP-specific integration code. No public source
text is copied.

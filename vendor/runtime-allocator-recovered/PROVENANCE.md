# Runtime allocator provenance

The range splitting, address-ordered free-list insertion, and adjacent-range
coalescing contracts were checked against these public allocators:

- FreeRTOS `heap_4.c`, commit
  `ce221a8bb468e462ca6b435cef66a9636e00baf4`, MIT:
  https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/portable/MemMang/heap_4.c
- Zephyr `lib/heap/heap.c`, commit
  `0d65ce46dda0626164503c50f37e6e1f59d310a9`, Apache-2.0:
  https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/lib/heap/heap.c

The AVP allocator route, Firebloom tuple, metadata arena, and support callback
ABIs are original OpenAVB adapters written to the expected ROM interface. No
source text from either public allocator was copied.

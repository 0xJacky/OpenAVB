# Runtime task lifecycle provenance

The module reuses OpenAVB's existing `RuntimeTask` state model and
scheduler-ready contract. Its public lifecycle reference is FreeRTOS-Kernel at
commit `ce221a8bb468e462ca6b435cef66a9636e00baf4` (MIT):

- https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/tasks.c
- https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/portable/GCC/ARM_AARCH64/port.c
- https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/LICENSE.md

The public contracts used here are a startup trampoline that invokes the task
entry and routes its return value to task exit, plus explicit ready-state queue
publication. OpenAVB implements the AVP interface's 0x5b0 task object,
Firebloom bounded capability tuple, projected-start route validation, and fatal
bounds handling. No FreeRTOS source text is copied into the implementation.

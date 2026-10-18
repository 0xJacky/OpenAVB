# Runtime scheduler public-source provenance

The scheduler implements the AVP interface's 0x5b0 task record, 0x80 timer
event, Firebloom bounds, callback metadata, source token, and fatal integrity
contracts. The ready FIFO and stable deadline queue were reviewed against
these public implementations. No public source text is copied into this
module.

## FreeRTOS scheduler reference

- Project: FreeRTOS-Kernel
- Repository: https://github.com/FreeRTOS/FreeRTOS-Kernel
- Commit: `ce221a8bb468e462ca6b435cef66a9636e00baf4`
- Tree: `4931f7d76da2edd0e81919de4bfd2cb13ded8051`
- File: `tasks.c`
- File blob: `43c9063c0912dc262893ee0880345ff48551f448`
- License: MIT
- License blob: `9cf106272ac3b56b0c4c80218e8fc10a664ca5f4`
- Source URL: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/tasks.c
- License URL: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/LICENSE.md

The referenced contracts are FIFO publication among equal-priority ready
tasks, delayed-task ordering, removal before wake processing, and explicit
current-task selection.

## Zephyr timeout reference

- Project: Zephyr
- Repository: https://github.com/zephyrproject-rtos/zephyr
- Commit: `0d65ce46dda0626164503c50f37e6e1f59d310a9`
- Tree: `db3fed4eddf0cb918cad68389bdf5ce64647c1f6`
- Scheduler file: `kernel/sched.c`
- Scheduler blob: `8687602dab426a1b3ea706e2246ef97d3b53c687`
- Timeout file: `kernel/timeout.c`
- Timeout blob: `e4c467529c5df15e8f8c9f730e1c23574d066c16`
- Delta-list file: `kernel/timeout_list.h`
- Delta-list blob: `c6615545fcf9b1571247c68fe1c92a801ecb595e`
- License: Apache-2.0
- Scheduler URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/sched.c
- Timeout URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/timeout.c
- Delta-list URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/timeout_list.h
- License URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The referenced contracts are ordered timeout insertion, unlink-before-callback
dispatch, and reprogramming from the first future deadline.

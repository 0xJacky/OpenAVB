# Runtime SVC dispatch public-source provenance

The module implements the AVP interface's SVC-number permission bitmap,
seven-argument callback ABI, bounded diagnostic copies, type-6 service
capability, counted-vector validation, page operation constants, and fatal
contracts.
No public source text is copied into this implementation.

## Zephyr userspace and syscall validation

- Project: Zephyr
- Repository: https://github.com/zephyrproject-rtos/zephyr
- Commit: `0d65ce46dda0626164503c50f37e6e1f59d310a9`
- Tree: `db3fed4eddf0cb918cad68389bdf5ce64647c1f6`
- Syscall handler: `include/zephyr/internal/syscall_handler.h`
- Syscall-handler blob: `a2961a1dc7f259da582591d038e89ef8b3a272cc`
- Userspace handler: `kernel/userspace/userspace_handler.c`
- Userspace-handler blob: `3da40f008875ed3076814a0fc3e30fd3e572cefb`
- License blob: `8f71f43fee3f78649d238238cbde51e6d7055c82`
- License: Apache-2.0
- Syscall URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/include/zephyr/internal/syscall_handler.h
- Userspace URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/userspace/userspace_handler.c
- License URL: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The referenced public contracts are explicit syscall-number dispatch,
per-object permission validation, bounded userspace memory access, and visible
fatal rejection of unauthorized calls.

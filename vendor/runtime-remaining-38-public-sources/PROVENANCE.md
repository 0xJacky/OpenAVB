# Runtime remaining modules provenance

The implementation follows the AVP interface's record layouts, status
transitions, fixed four-value endpoint payloads, FIFO wake order, 16 KiB stack
pages, DER payload-property traversal, and Firebloom bounds failures. Public
implementations were used as behavioral references. No public source text was
copied.

## Zephyr message queues, wait queues, and threads

- Project: Zephyr
- Repository: https://github.com/zephyrproject-rtos/zephyr
- Commit: `0d65ce46dda0626164503c50f37e6e1f59d310a9`
- License: Apache-2.0
- Message queue blob: `2f4721479507e0cda72455bfac0fac6d8143676f`
- Scheduler blob: `8687602dab426a1b3ea706e2246ef97d3b53c687`
- Thread blob: `34075676a36155bbc6b5f4b6767a5ecd5d567846`
- Message queue source: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/msg_q.c
- Scheduler source: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/sched.c
- Thread source: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/kernel/thread.c
- License: https://github.com/zephyrproject-rtos/zephyr/blob/0d65ce46dda0626164503c50f37e6e1f59d310a9/LICENSE

The referenced contracts are bounded FIFO buffering, direct waiter handoff,
explicit timeout results, FIFO wakeup, and separate thread record and stack
initialization.

## Arm Trusted Firmware-A handoff and image descriptor model

- Project: Trusted Firmware-A
- Repository: https://github.com/ARM-software/arm-trusted-firmware
- Commit: `4736daa241c9e9349d4931a44e95991e75a19dd1`
- License: BSD-3-Clause
- Image source blob: `0626de9d0d8d8538a20412daac70ab5bd950866f`
- License blob: `3ec3b74f9e1da85083317de872cf3dbb0de9ae60`
- Image source: https://github.com/ARM-software/arm-trusted-firmware/blob/4736daa241c9e9349d4931a44e95991e75a19dd1/common/desc_image_load.c
- License: https://github.com/ARM-software/arm-trusted-firmware/blob/4736daa241c9e9349d4931a44e95991e75a19dd1/license.rst

The referenced contract is a registry of bounded image sources keyed by image
identity, with loading separated from record registration.

## Apple libDER

- Project: Apple Security libDER
- Repository: https://github.com/apple-oss-distributions/Security
- Tag: `Security-57740.60.18`
- Commit: `db455f62ee15e6adf20afe7c2e132321d2d0759c`
- License: APSL-2.0
- Source: https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/DER_Decode.c
- License: Apple Public Source License 2.0, https://opensource.apple.com/apsl/

The DER module follows definite-length TLV bounds checking (as specified by
ITU-T X.690) and forwards integer and octet-string property values through a
typed observer. No libDER source text is copied.

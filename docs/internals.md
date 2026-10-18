# Internals

## Toolchain

| Tool | Used for |
| --- | --- |
| LLVM `clang`, `llvm-objcopy`, `llvm-readobj` (`/opt/homebrew/opt/llvm/bin`, override with `LLVM_PREFIX=`) | cross-compiling the freestanding `aarch64-none-elf` images |
| `ld.lld` (`/opt/homebrew/bin/ld.lld`) | linking the ROM and payload images |
| host `clang` (Xcode Command Line Tools; override with `HOST_CC=`) | building the host test binaries |
| `uv` (with a Python 3 it can run) | the scripts under `tools/` (standard library only) |
| `rg` (ripgrep) | symbol and instruction-encoding assertions in `make verify` |
| `r2` (radare2) | instruction-encoding assertions on the built images in `make verify` |
| `shasum`, `od`, BSD `stat` | size and digest checks (macOS base system) |

Nothing outside this repository is needed to build or verify.

`make verify` builds the ROM, the source iBSS, the normal-boot fixture payload
and the probe ROM images; builds and runs every host test; checks symbol
placement and selected instruction encodings in the linked images; and checks
image sizes and the source-image CRCs. Every `tests/**/<name>_tests.c` builds
to `build/<name>_tests` from the same sources as the ROM.

## Source images

`build/SourceIBSS.bin` and `build/NormalBootPayload.bin` share one container
format: a 32-byte header (`'SBIS'`, version 1, header size, image size, entry
offset, identity `0x5352434942535331`, CRC-32 of the image with the CRC field
zeroed); `tools/finalize_source_image.py verify` checks it. `SourceIBSS.bin`
loads at `0x7006c000` and enters at `0x7006c800`.

`SourceIBSS.bin` is the matching DFU payload. A copy with one corrupted instruction is refused and the device stays in DFU; the valid image runs, adds the iBSS-owned `IBSS:EXECUTED` token to its USB serial string, and accepts a `0x800`-byte Recovery endpoint-4 transfer plus ZLP. The real path instead accepts a personalized IMG4 iBSS: it validates the bounded DER structure, the `ibss` FourCC, the compressed payload size and SHA-256, the decoded size and SHA-256 (from the uploaded stage-0 identity), and the entry address.

## Tools

| Tool | Purpose |
| --- | --- |
| `tools/patchup.py` | Host side of the patch-store contract: `build` packs a stage blob from a JSON list of `(offset, instruction)` rows plus an optional image identity; `send` uploads it to a device in DFU or Recovery over the `0x70`..`0x73` vendor requests (needs `pymobiledevice3`). |
| `tools/finalize_source_image.py` | Writes (`finalize`) or checks (`verify`) the CRC-32 of a source image container. Used by the build. |
| `tools/gen_platform_profile.py` | Generates `platform_profile.h` from the platform description. |
| `tools/check_patch_uploader_contract.py` | Cross-checks `patchup.py` against the ROM's patch store. |
| `tools/verify_layout.py` | Enforces the source layout rules. |

`make verify` runs three of them as checks:

- `tools/verify_layout.py` keeps every implementation and test inside a
  subsystem owner, requires unique source basenames (ROM object names are
  flat), and validates each root-level `include/avp/*.h` forwarding header.
- `tools/check_patch_uploader_contract.py` builds stage blobs with the host
  uploader `tools/patchup.py` and replays them through the ROM's own
  `patch_store.c` (`build/patch_store_tests --blob FILE --stage N`), so the
  112-byte `AVPPatchStageHeader`, the `'PTCH'` seal, the stage ordinals and the
  USB vendor requests `0x70`..`0x73` cannot drift apart on one side only.
- `tools/gen_platform_profile.py --check` confirms the committed
  `include/avp/platform/platform_profile.h` is what the generator produces
  from the platform description in `tests/platform/fixtures/platforms`. After
  editing that description, run `tools/gen_platform_profile.py` (via
  `uv run --no-project`) to regenerate the header; `--platforms DIR` or
  `OPENAVB_PLATFORMS_DIR` points it at another directory of descriptions.

## Probe images

Each `build/OpenAVB.<name>-probe.bin` replaces the ROM to exercise one
subsystem on a machine that implements the contract; each reports through the
platform progress words and pvpanic.

The task scheduler uses the `0x370`-byte architectural frame inside a `0x5b0`-byte TCB. SVC `0x41` validates task capabilities and enqueues initialized tasks, SVC `0x42` schedules ready tasks in FIFO order, and SVCs `0x22`/`0x23` update the selected frame's interrupt mask. SVC `0x43` arms the TCB's embedded timer event, while idle SVC `0x20` drains the deadline queue and waits through WFI. Type-5 Event SVCs `0x50`/`0x51`/`0x52` provide creation, FIFO wait, timeout, set, and clear semantics. The scheduler publishes the selected TCB through `TPIDR_EL1`, restores the selected domain and architectural state, and resumes through ERET. Synchronous service-owner calls use the common C2/C3/C4 exception scheduler with state-5 IPC waits and in-place replies. The scheduler probe image verifies capability activation, interrupt masks, two EL0 tasks across two FIFO rounds, and restoration of the boot task. The timer probe image verifies the asynchronous public-HVF VTimer-to-GIC-PPI27 FIQ path and the fixed idle task. The Event probe image verifies real Event SVC instructions, ERET scheduling, timeout delivery, signal-before-timeout cancellation, and masked-wait rejection.

The preflight probe executes command-table index 20 twice. Its test registry creates an absolute deadline 20,000 microseconds after lookup; a run measures at least that interval and observes one registry lookup across both commands.

The bootstrap probe requires payload `0xa0`, five nonzero command-4 words, and eight command-1 responses with opcode `0x65`. It also runs all-zero configuration and mismatched-finalize transcripts and requires both to stop through pvpanic before reporting completion.

The runtime-transport probe completes the bootstrap sequence, submits two command-0x10 requests through the provider-derived direct mailbox slots, assembles response payloads `0x89abcdef` and `0x01234567` into one 64-bit value, installs masked guard `0x0123456789ab00ef`, reads back `TPIDRRO_EL0 == UINT64_MAX`, and proves that response opcode `0x75`, a FULL eight-entry receive queue, and a two-second EMPTY timeout enter pvpanic before publishing completion.

The runtime-IPC probe fills the eight-qword endpoint with two four-qword requests, blocks two more callers in FIFO order, and requires receive/completion traces `0x1234`. It verifies waiter promotion, provider-priority C4 completion, provider-buffer isolation, preserved caller X0 values, cleared reply tokens, and an empty endpoint. Two negative images forge the active reply token and packet projection; each must reach pvpanic before its post-call marker.

The timer probe blocks the boot task through SVC `0x43`, runs a second EL0 task while the deadline is active, receives the public-HVF VTimer exit as Group-0 PPI27, saves the interrupted task through the FIQ vector, drains the timer queue, resumes through the fixed idle SVC `0x20` pump, and restores both continuations through ERET. Its witness requires four task switches, worker trace `0x101`, an empty timer/ready queue, and boot domain 1.

The Event probe creates auto-reset and manual-reset objects through SVC `0x50`. SVC `0x51` covers poll, infinite wait, finite timeout, and signal-before-timeout cancellation; SVC `0x52` covers clear, one-waiter FIFO wake, and broadcast wake. Its witness requires traces `0x123` and `0x45`, 24 task switches, empty timer/ready queues, and a separate masked-I/F blocking wait that reaches pvpanic.

The stack-protector probe changes the active guard from inside a compiler-protected frame. Its epilogue detects the epoch mismatch, calls `__stack_chk_fail`, and reaches pvpanic with stage `SSPF` before its completion marker can be written.

## Implementation notes

The source ROM starts at `0x00100000`, uses work RAM beginning at `0x70000000`, publishes the `{status_flags, boot_config, board_id, security_epoch}` boot context at `0x00400300`, and dispatches the task and service SVCs through one trap-frame owner. Status bit zero marks context initialization and bit one marks the cached boot nonce. Its runtime separates domain records from service tasks, supports multiple tasks in one domain, and creates dynamic domains from the `0x60` descriptor into exact `0x118` records. Domain registration publishes private, public, and primary-mapping capabilities after rejecting duplicate identities, duplicate domain IDs, primary physical overlaps, and optional virtual overlaps. Each service uses a type-4 endpoint with a bounded four-qword ring, a blocked-caller FIFO, and one provider receive buffer. C4 places completed callers on the ready tail and keeps the provider running so it can drain pending work before C3 blocks. Reset enables FP/SIMD before any task switch because the task ABI preserves Q8-Q15. The ROM performs the verified table-driven SEP startup exchange, initializes the SimpleVirtio USB device at `0x30100000`, and serves an Apple DFU control endpoint compatible with pymobiledevice3 `IRecv`. The SEP model preserves all 29 active protocol prefixes from Apple's 43-slot command table; the main startup path executes index 2 once and index 3 across five configuration words before entering USB. The dedicated finalize contract executes index 0 across eight matching rounds for callers that require that readiness proof. It also models Apple's 144-byte type-2 direct mailbox view, FULL/EMPTY control state, two-second response deadline, and descriptor-6 research-pattern entropy source. SEP receive deadlines use the architectural virtual counter in the descriptor's microsecond unit. A timeout enters the matching five-window recovery policy and polls every 100 microseconds. Preflight descriptors resolve typed registry key `0x2000`, interpret value `210000` in the absolute-microsecond clock domain, and perform the wait once. Its boot-source policy selects `virt_firmware` for attempt zero and DFU for subsequent or forced attempts. Its PID, board identity, chip revision, security domain, ECID, product string, and boot nonces come from the public-HVF platform contract. The platform entropy owner provides 48-byte seed, enable, request, control, and reseed transitions. Runtime nonces and stack guards use the pinned Mbed TLS SHA-384 HMAC_DRBG implementation, preserving deterministic output for a fixed platform seed. The shared boot-image evaluator consumes the `0x30`-byte image descriptor, validates bounded IMG4 or standalone IM4P DER, recognizes context-`[0]` IM4M tickets, handles manifestless Memz payloads, and enforces caller FourCC policy. The real path then verifies the pinned `ibss` artifact, decodes LZFSE into `0x7006c000`, and transfers execution after manifestation reset.

DFU identity finalization runs after SEP bootstrap. `NONC` is the first 32
bytes of SHA-384 over the cached eight-byte boot nonce. `SNON` is the exact
little-endian byte representation of the five SEP configuration words.

Early runtime initialization also executes the boot-owner memory bootstrap. The matching profile publishes `0x70058000..0x70064000`; the typed alternate API validates the exact `0xb8` descriptor, optional storage pairs, 16 KiB page-table pool, `0x40`-byte page records, replicated map entries, and encoded reserved spans before publishing state.

Support-library integration retains the current-domain primary/fallback allocator selector, exact `0x68` route and `0x20` allocation-node layouts, bounded release ownership, secure allocation clearing, page-mapping release, the fixed variadic diagnostic callback, and three observable Firebloom panic classes. The physical-memory registry preserves the eleven matching `0x40` records, region IDs and attributes, active aliases, live/defined predicates, checked upper bounds, and bounded region clearing. The address-space owner publishes exact `0x20` type-7 mapping objects, validates audience and region coherence, rejects virtual overlaps, reuses compatible mappings, and writes attached mapping handles into physical-region descriptor bits 48 through 63. Fatal records preserve the inherited detail, message pointer, and call site before pvpanic termination.

For normal boot, the host exposes one bounded firmware image through the root SimpleVirtio block window at `0x30000000`. The source ROM initializes device ID 2, reads the image prefix and complete sector-rounded payload through the three-segment block request ABI, selects candidate type 14, validates the same source/IMG4 formats used by DFU, emits the `HOFF` entropy boundary, and transfers execution. The block domain initializer follows the locality split: a local or absent domain uses direct MMIO, while a registered remote owner receives the exact `storage` method-`0x18` serialized initialization packet through SVC `0xc2`. The generic RPC codec emits the `0x20`-byte indexed field headers, aligned payloads, per-field tails, and final packet sentinel; the storage request occupies `0xa0` bytes. Owner-resident callbacks perform block I/O; the Source runtime assigns explicit extension methods `0x4001` and `0x4002` to carry those callbacks across its synchronous service boundary. The block probe image verifies the remote-owner initialization plus a write/read round trip; a normal boot with the fixture payload (`build/NormalBootPayload.bin`) on the block device requires the block-loaded payload's `NORM` marker and pvpanic witness.

Reset assembly installs the bootstrap guard before entering the first C frame. After SEP bootstrap, `stack_guard.c` replaces it with a SHA-384 HMAC_DRBG value. A null reset-entry profile continues to select two SEP research-pattern words through the direct runtime transport. The caller applies mask `0xffffffffffff00ff`, stages the guard at `0x7003ca70`, copies it to `__stack_chk_guard` at `0x7002a230`, and initializes `TPIDRRO_EL0` to all ones. ROM C code uses Clang strong stack protection and machine outlining; `__stack_chk_fail` routes every compiler-detected mismatch through pvpanic. iBSS and test payloads retain their independent freestanding compiler profile.

The control implementation covers device/configuration/string descriptors, per-boot entropy-backed nonce and identity strings, address/configuration state, DFU download/status/state/abort transitions, reset completion, Recovery enumeration, and endpoint-4 bulk acceptance. Both image paths validate the Apple DFU footer and transport CRC before returning manifestation states 6 and 7. The source fixture additionally validates its project header and content CRC. The real path validates the bounded DER structure, `ibss` FourCC, compressed payload size and SHA-256, decoded size and SHA-256, and fixed entry address.

The custom booter's security boundary is artifact identity. Guest code validates the executable payload before handoff. The Host probe validates that the ticket's `ibss` digest names the same IM4P. The real iBSS applies Apple's native ApNonce and IM4M policy when it accepts iBEC. Full Apple certificate-chain validation belongs to the secure-boot policy track.

## Work RAM layout

Writable state and queue storage use fixed RAM IPAs:

| IPA | Owner |
| --- | --- |
| `0x70001000` | queue-0 descriptor |
| `0x70002000` | queue-0 event buffer |
| `0x70003000` | queue-1 descriptor |
| `0x70004000` | queue-1 response buffer |
| `0x70005000` | USB DFU state |
| `0x70006000` | block request descriptor chain |
| `0x70006100` | block request header and status |
| `0x70007000` | block read/write Gate buffer |
| `0x70008000..0x70010000` | TinyUSB device-core and DFU state |
| `0x70010000..0x70014000` | Platform entropy, HMAC_DRBG, and Mbed TLS allocator state |
| `0x7006c000` | decoded iBSS image and entry region |
| `0x70074000..0x70140000` | iLLB resident image and zeroed BSS (plaintext payloads) |
| `0x70300000..0x703c0000` | LZFSE decode scratch |
| `0x1fc050000..0x1fc450000` | stock t8110 iLLB lzfse decode target (SRAM aperture) |
| `0x70300000` | source iBSS writable data |
| `0x7003cad0` | fixed boot TCB |
| `0x7003d0a0` | ready count |
| `0x7003d160` | interrupted-task resume slot |
| `0x7003d1e0` | ready FIFO sentinel |
| `0x7003d220` | timer queue sentinel |
| `0x7003d7c0` | fixed idle TCB |
| `0x703d0000` | timer Gate worker stack |
| `0x701c0000` | idle task stack top |
| `0x701c0000` | stack bottom |
| `0x701d0000` | initial stack top |
| `0x71000000` | personalized IMG4 upload buffer |

## Source layout

The implementation uses an owner-oriented layout. Primary headers under `include/avp` mirror the source and test hierarchy:

```text
include/avp/                 src/                         tests/
├── crypto/                  ├── crypto/                  ├── crypto/
├── devices/                 ├── devices/                 ├── devices/
├── image/                   ├── image/                   ├── image/
├── platform/                ├── platform/                ├── platform/
├── protocol/sep/            ├── protocol/sep/            ├── protocol/sep/
└── runtime/                 └── runtime/                 └── runtime/
    ├── boot/                    ├── boot/                    ├── boot/
    ├── exception/               ├── exception/               ├── exception/
    ├── firebloom/               ├── firebloom/               ├── firebloom/
    ├── ipc/                     ├── ipc/                     ├── ipc/
    ├── memory/                  ├── memory/                  ├── memory/
    ├── prng/                    ├── prng/                    ├── prng/
    ├── support/                 ├── support/                 ├── support/
    └── task/                    └── task/                    └── task/
```

Root-level `include/avp/*.h` files are forwarding headers for existing consumers. New internal code includes the hierarchical primary header. The build discovers project sources recursively, keeps source basenames unique, and preserves flat ROM object names so the linker placement rules remain stable. Third-party code and its provenance remain isolated under `vendor/`.

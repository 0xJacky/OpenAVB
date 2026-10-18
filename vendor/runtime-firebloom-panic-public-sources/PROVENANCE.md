# Runtime Firebloom panic provenance

The panic formatter reuses the nanoprintf-backed runtime format pipeline. The
scratch-frame layout follows the public AArch64 procedure-call standard, while
panic descriptors and failure-hook routing follow the AVP-specific ROM
interface.

- Project: nanoprintf
- Repository: https://github.com/charlesnicholson/nanoprintf
- Release: `v0.8.0`
- Commit: `115c916031ef51b73c5c373852fa550eaf134d49`
- Tree: `133dc919cdaa8780ebb36d36267e09a9c0260cf5`
- License: 0BSD OR Unlicense
- Source URL: https://github.com/charlesnicholson/nanoprintf/blob/115c916031ef51b73c5c373852fa550eaf134d49/nanoprintf.h
- Project: Arm ABI documents
- Repository: https://github.com/ARM-software/abi-aa
- Commit: `ee4b3c12d57c8424ff60c2ae56e10690d0604ab6`
- Tree: `61915eaa3c2a92e9e769a6f26ee159b79fb8cea9`
- Document: `aapcs64/aapcs64.rst`
- Document blob: `6ea5432e8d2a1e569251590b978ec0574ec630b1`
- License: CC-BY-SA-4.0
- License blob: `83edfc5018be050258319aded46246ade6853fbb`
- Source URL: https://github.com/ARM-software/abi-aa/blob/ee4b3c12d57c8424ff60c2ae56e10690d0604ab6/aapcs64/aapcs64.rst
- License URL: https://github.com/ARM-software/abi-aa/blob/ee4b3c12d57c8424ff60c2ae56e10690d0604ab6/LICENSE.md

No public source text is copied into this adapter.

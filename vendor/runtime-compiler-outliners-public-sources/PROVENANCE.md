# Runtime compiler-outliner provenance

The implementation provides the shared helper entry points that a compiler's
outlining pass produces. LLVM's Machine Outliner was reviewed to confirm the
calling and stack-layout constraints of extracted instruction sequences.

| Project | Commit | License | Reviewed implementation |
| --- | --- | --- | --- |
| LLVM | `37dcccc7367f1d63402db356497cdf91fb85ef77` | Apache-2.0 WITH LLVM-exception | `llvm/lib/CodeGen/MachineOutliner.cpp` |

Upstream URL:

- https://github.com/llvm/llvm-project/blob/37dcccc7367f1d63402db356497cdf91fb85ef77/llvm/lib/CodeGen/MachineOutliner.cpp

The AArch64 frame leaves keep the stack offsets the ROM interface expects.
Register move helpers use explicit C aggregates so their state-transfer
contracts remain callable and testable. No LLVM source text is copied.

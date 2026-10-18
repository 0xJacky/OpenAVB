# Runtime Firebloom layout provenance

The AVP layout ABI uses packed 16-byte entries with a byte offset, element
count, layout kind, structural flag, and element type. Its validation and
cached suffix construction are implemented locally.

LLVM's `DataLayout` and `StructLayout` interfaces were reviewed as the public
reference for ordered member offsets, allocation sizes, and layout validation:

- Repository: https://github.com/llvm/llvm-project
- Commit: `37dcccc7367f1d63402db356497cdf91fb85ef77`
- File: https://github.com/llvm/llvm-project/blob/37dcccc7367f1d63402db356497cdf91fb85ef77/llvm/include/llvm/IR/DataLayout.h
- License: Apache-2.0 WITH LLVM-exception

The public API shape differs from the Firebloom tuple and relation ABI, so no
LLVM source text is copied into this implementation.

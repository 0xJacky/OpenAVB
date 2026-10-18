# Runtime memory policy provenance

This module implements the memory-policy entry points: region identifier
tables, the three 80-byte membership bitmaps, bootstrap-record layout,
address-profile windows, mapping flags, and terminal bounds behavior.

Mapping operations use a thin transport over the existing OpenAVB
page-table and physical-memory modules. The policy keeps 16 KiB page-table
semantics, TTBR regime selection, MAIR attribute selection, overflow-safe
half-open range validation, and observable panic contracts.

The implementation is original project code and imports no third-party code.
This provenance record documents its technical sources and makes no independent
license grant.

## Public specifications

- Arm Memory Management:
  https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/LearnTheArchitecture-MemoryManagement-101811_0100_00_en.pdf
- Arm A-profile Architecture Reference Manual:
  https://developer.arm.com/documentation/ddi0487/latest/

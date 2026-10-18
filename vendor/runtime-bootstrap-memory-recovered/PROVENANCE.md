# Runtime bootstrap memory provenance

This module implements the bootstrap-memory entry points: the expected ABI,
constants, region-record offsets, flag transitions, descriptor validation,
mapping requests, reverse shared-capability traversal, and fatal contracts.

The implementation composes the existing OpenAVB physical-memory,
page-table, page-attribute, memory-policy, domain, task, and Firebloom modules.
It contains original project code and imports no third-party source. This
provenance record documents its technical sources and makes no independent
license grant.

## Public specifications and implementations

- Arm A-profile Architecture Reference Manual:
  https://developer.arm.com/documentation/ddi0487/latest/
- Arm Memory Management:
  https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/LearnTheArchitecture-MemoryManagement-101811_0100_00_en.pdf
- EDK2 PEI memory services (BSD-2-Clause-Patent):
  https://github.com/tianocore/edk2/blob/2970e5699ba6267f3384ffab20f96647578aebc8/MdeModulePkg/Core/Pei/Memory/MemoryServices.c

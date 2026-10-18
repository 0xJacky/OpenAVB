# Virtual-firmware redundant-header provenance

This module implements HUFA redundant-header discovery and generation
selection: each redundant copy is read and validated independently, at most
the protocol-defined copy count is retained, and the newest valid generation
is selected. It is original OpenAVB code and contains no third-party source.

## Behavior contract

- Scan 32 locations at a 4 KiB stride and validate 64-byte duplicated HUFA
  headers (magic `0x41465548`, duplicated 32-byte halves); retain up to two
  candidates and their storage offsets.
- Prefer the greater generation, keep the first copy on ties, publish
  selection diagnostics, and return the selected source offset.
- Report no-header progress `0x40130005`; pair selection progress
  `0x00130008` with the selected source offset.

Exported functions:

- `boot_virtual_firmware_select_latest_source_offset`
- `boot_virtual_firmware_scan_redundant_headers`
- `boot_virtual_firmware_log_generated_location`
- `boot_virtual_firmware_prepare_selection_progress`
- `boot_virtual_firmware_prepare_provider_read`
- `boot_virtual_firmware_compare_header_magic`
- `boot_virtual_firmware_no_header_progress_code`
- `boot_virtual_firmware_dispatch_provider_read`
- `boot_virtual_firmware_clear_header_candidates`

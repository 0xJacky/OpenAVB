# Platform boot-policy owner provenance

This owner implements the platform policy record: capability flags, tracked
boot-memory bounds, the optional 48-byte entropy seed, reset initialization,
and immutable platform policy hooks. It is a thin bindable transport around
the platform policy bit layout and fixed memory record, and reuses the
existing OpenAVB production-mode owner. It is original OpenAVB code and
contains no third-party source.

## Behavior contract

- Reset flags and bounds, optionally clearing the fixed boot-memory window.
- Reset or expand the tracked interval with checked addition; publish
  overflow through the fatal path.
- Validate overflow-free spans against the tracked bounds.
- Promote bit-four requests to the bit-five profile, which clears bits 18,
  19, and 29.
- Publish or clear the optional 48-byte entropy seed and the entropy context
  word.
- Provide a platform watchdog service point for long boot-memory operations.

Exported functions:

- `platform_watchdog_service_noop`
- `platform_boot_policy_reset`
- `platform_boot_policy_memory_bounds_update`
- `platform_boot_policy_memory_bounds_overflow_fatal`
- `platform_boot_policy_flags_accept_and_promote_request`
- `platform_boot_policy_select_flag5_profile`
- `platform_boot_policy_memory_span_allowed`
- `platform_boot_policy_flag24_set`
- `platform_boot_policy_flag22_is_set`
- `platform_boot_policy_compare_production_mode_byte`
- `platform_boot_policy_set_entropy_seed_span`
- `platform_boot_policy_set_entropy_context_word`
- `platform_boot_policy_flag25_set`
- `platform_boot_policy_flag27_set`
- `platform_boot_policy_flag30_set`
- `platform_boot_policy_flag42_set`
- `platform_boot_policy_flag40_is_clear`
- `platform_boot_source_attempt_completion_progress_is_enabled`
- `platform_boot_normal_image_type_illb`
- `platform_boot_policy_flag27_default_is_set`
- `platform_memory_policy_range_is_exempt`
- `j_platform_boot_source_attempt_completion_progress_is_enabled`
- `j_platform_boot_policy_memory_span_allowed`

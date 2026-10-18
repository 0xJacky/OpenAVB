#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"

#if __has_attribute(retain)
#define PLATFORM_BOOT_POLICY_PUBLIC __attribute__((used, retain))
#else
#define PLATFORM_BOOT_POLICY_PUBLIC __attribute__((used))
#endif

enum {
  PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE = 0x70074000,
  PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE = 0x0038c000,
  PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE = 48,
  PLATFORM_BOOT_POLICY_IMAGE_TYPE_ILLB = 0x696c6c62,
};

enum {
  PLATFORM_BOOT_POLICY_FLAG4 = UINT64_C(1) << 4,
  PLATFORM_BOOT_POLICY_FLAG5 = UINT64_C(1) << 5,
  PLATFORM_BOOT_POLICY_FLAG16 = UINT64_C(1) << 16,
  PLATFORM_BOOT_POLICY_FLAG18 = UINT64_C(1) << 18,
  PLATFORM_BOOT_POLICY_FLAG19 = UINT64_C(1) << 19,
  PLATFORM_BOOT_POLICY_FLAG20 = UINT64_C(1) << 20,
  PLATFORM_BOOT_POLICY_FLAG22 = UINT64_C(1) << 22,
  PLATFORM_BOOT_POLICY_FLAG24 = UINT64_C(1) << 24,
  PLATFORM_BOOT_POLICY_FLAG25 = UINT64_C(1) << 25,
  PLATFORM_BOOT_POLICY_FLAG26 = UINT64_C(1) << 26,
  PLATFORM_BOOT_POLICY_FLAG27 = UINT64_C(1) << 27,
  PLATFORM_BOOT_POLICY_FLAG28 = UINT64_C(1) << 28,
  PLATFORM_BOOT_POLICY_FLAG29 = UINT64_C(1) << 29,
  PLATFORM_BOOT_POLICY_FLAG30 = UINT64_C(1) << 30,
  PLATFORM_BOOT_POLICY_FLAG40 = UINT64_C(1) << 40,
  PLATFORM_BOOT_POLICY_FLAG42 = UINT64_C(1) << 42,
};

typedef struct {
  uint64_t flags;
  uint8_t auxiliary_state;
  uint8_t reserved_009[7];
  uint64_t memory_lower_bound;
  uint64_t memory_upper_bound;
  uint8_t reserved_020[0x120];
  uint8_t entropy_seed[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE];
} PlatformBootPolicyRecord;

_Static_assert(offsetof(PlatformBootPolicyRecord, memory_lower_bound) == 0x10,
               "boot-policy lower bound offset");
_Static_assert(offsetof(PlatformBootPolicyRecord, memory_upper_bound) == 0x18,
               "boot-policy upper bound offset");
_Static_assert(offsetof(PlatformBootPolicyRecord, entropy_seed) == 0x140,
               "boot-policy entropy seed offset");

typedef void (*PlatformBootPolicyClearMemory)(
  void *context,
  uintptr_t address,
  size_t size
);
typedef void (*PlatformBootPolicyWatchdog)(void *context);

typedef struct {
  volatile PlatformBootPolicyRecord *record;
  volatile uint32_t *entropy_context;
  PlatformBootPolicyClearMemory clear_memory;
  PlatformBootPolicyWatchdog service_watchdog;
  void *context;
} PlatformBootPolicyTransport;

extern const RuntimeFirebloomType platform_boot_policy_byte_type;

void platform_boot_policy_bind_transport(PlatformBootPolicyTransport transport);
void platform_boot_policy_reset_transport(void);

PLATFORM_BOOT_POLICY_PUBLIC void platform_watchdog_service_noop(void);
PLATFORM_BOOT_POLICY_PUBLIC int
platform_boot_policy_reset(bool clear_boot_memory);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_memory_bounds_update(
  const void *current,
  const void *lower_bound,
  const void *upper_bound,
  const RuntimeFirebloomType *type_context,
  size_t byte_count,
  bool reset_bounds
);
PLATFORM_BOOT_POLICY_PUBLIC _Noreturn void
platform_boot_policy_memory_bounds_overflow_fatal(void);
PLATFORM_BOOT_POLICY_PUBLIC bool
platform_boot_policy_flags_accept_and_promote_request(uint64_t requested_flags);
PLATFORM_BOOT_POLICY_PUBLIC void
platform_boot_policy_select_flag5_profile(void);
PLATFORM_BOOT_POLICY_PUBLIC bool platform_boot_policy_memory_span_allowed(
  uint64_t current,
  const void *lower_bound,
  const void *upper_bound,
  const RuntimeFirebloomType *type_context,
  size_t byte_count
);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_flag24_set(bool enabled);
PLATFORM_BOOT_POLICY_PUBLIC bool platform_boot_policy_flag22_is_set(void);
PLATFORM_BOOT_POLICY_PUBLIC uint16_t
platform_boot_policy_compare_production_mode_byte(uint8_t expected);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_set_entropy_seed_span(
  const void *current,
  const void *lower_bound,
  const void *upper_bound,
  const RuntimeFirebloomType *type_context
);
PLATFORM_BOOT_POLICY_PUBLIC void
platform_boot_policy_set_entropy_context_word(uint32_t context);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_flag25_set(bool enabled);
void platform_boot_policy_apply_entropy_identity_state(void);
int platform_boot_policy_publish_entropy_request(void);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_flag27_set(void);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_flag30_set(void);
PLATFORM_BOOT_POLICY_PUBLIC void platform_boot_policy_flag42_set(void);
PLATFORM_BOOT_POLICY_PUBLIC bool platform_boot_policy_flag40_is_clear(void);
PLATFORM_BOOT_POLICY_PUBLIC bool
platform_boot_source_attempt_completion_progress_is_enabled(void);
PLATFORM_BOOT_POLICY_PUBLIC uint32_t platform_boot_normal_image_type_illb(void);
PLATFORM_BOOT_POLICY_PUBLIC bool
platform_boot_policy_flag27_default_is_set(void);
PLATFORM_BOOT_POLICY_PUBLIC bool
platform_memory_policy_range_is_exempt(
  uint64_t lower_bound,
  uint64_t upper_bound
);
PLATFORM_BOOT_POLICY_PUBLIC bool
j_platform_boot_source_attempt_completion_progress_is_enabled(void);
PLATFORM_BOOT_POLICY_PUBLIC bool j_platform_boot_policy_memory_span_allowed(
  uint64_t current,
  const void *lower_bound,
  const void *upper_bound,
  const RuntimeFirebloomType *type_context,
  size_t byte_count
);

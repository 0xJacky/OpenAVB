#include "avp/platform/platform_boot_policy_owner.h"

#include "avp/platform/platform_identity_config_owner.h"
#include "avp/platform/platform_identity_status_nonce_owner.h"
#include "avp/platform/platform_entropy_transport_owner.h"
#include "avp/runtime/memory/runtime_memory.h"

enum {
  PLATFORM_BOOT_POLICY_RECORD_ADDRESS = 0x7002ac58,
  PLATFORM_BOOT_POLICY_ENTROPY_CONTEXT_ADDRESS = 0x7002ac54,
};

static const uint64_t platform_boot_policy_initial_flags = UINT64_C(0x222c0000);
static const uint64_t platform_boot_policy_flag5_profile_mask =
    UINT64_C(0xffffffffdff3ffdf);

const RuntimeFirebloomType platform_boot_policy_byte_type = {
    .identity = 0x00144f18,
    .element_size = sizeof(uint8_t),
    .bytewise_copyable = true,
};

#if defined(__ELF__)
/* Boot-policy test transport state shares the mapped platform PRNG window. */
#define PLATFORM_BOOT_POLICY_STATE                                             \
  __attribute__((section(".runtime_prng.state"), used))
#else
#define PLATFORM_BOOT_POLICY_STATE
#endif

static PlatformBootPolicyTransport bound_transport PLATFORM_BOOT_POLICY_STATE;

static _Noreturn void platform_boot_policy_contract_failure(void) {
  runtime_firebloom_panic();
}

static void platform_boot_policy_default_clear(
    void *context,
    uintptr_t address,
    size_t size
) {
  (void)context;
  volatile uint8_t *bytes = (volatile uint8_t *)address;
  for (size_t index = 0; index < size; ++index) {
    bytes[index] = 0;
  }
}

static void platform_boot_policy_default_watchdog(void *context) {
  (void)context;
  platform_watchdog_service_noop();
}

static PlatformBootPolicyTransport platform_boot_policy_transport(void) {
  if (bound_transport.record != 0) {
    return bound_transport;
  }
  return (PlatformBootPolicyTransport){
      .record = (volatile PlatformBootPolicyRecord *)
          PLATFORM_BOOT_POLICY_RECORD_ADDRESS,
      .entropy_context =
          (volatile uint32_t *)PLATFORM_BOOT_POLICY_ENTROPY_CONTEXT_ADDRESS,
      .clear_memory = platform_boot_policy_default_clear,
      .service_watchdog = platform_boot_policy_default_watchdog,
  };
}

static void platform_boot_policy_replace_flag(uint64_t flag, bool enabled) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  uint64_t flags = transport.record->flags & ~flag;
  if (enabled) {
    flags |= flag;
  }
  transport.record->flags = flags;
}

static void platform_boot_policy_require_seed_span(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  const uintptr_t current_address = (uintptr_t)current;
  const uintptr_t lower_address = (uintptr_t)lower_bound;
  const uintptr_t upper_address = (uintptr_t)upper_bound;
  if (type_context == 0 || !type_context->bytewise_copyable ||
      current_address < lower_address || current_address > upper_address ||
      PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE >
          upper_address - current_address) {
    platform_boot_policy_contract_failure();
  }
}

void platform_boot_policy_bind_transport(
    PlatformBootPolicyTransport transport) {
  if (transport.record == 0 || transport.entropy_context == 0 ||
      transport.clear_memory == 0 || transport.service_watchdog == 0) {
    platform_boot_policy_contract_failure();
  }
  bound_transport = transport;
}

void platform_boot_policy_reset_transport(void) {
  bound_transport = (PlatformBootPolicyTransport){0};
}

void platform_watchdog_service_noop(void) {}

bool platform_boot_source_attempt_completion_progress_is_enabled(void) {
  return false;
}

bool j_platform_boot_source_attempt_completion_progress_is_enabled(void) {
  return platform_boot_source_attempt_completion_progress_is_enabled();
}

uint32_t platform_boot_normal_image_type_illb(void) {
  return PLATFORM_BOOT_POLICY_IMAGE_TYPE_ILLB;
}

bool platform_boot_policy_flag27_default_is_set(void) { return true; }

bool platform_memory_policy_range_is_exempt(
    uint64_t lower_bound,
    uint64_t upper_bound
) {
  (void)lower_bound;
  (void)upper_bound;
  return false;
}

int platform_boot_policy_reset(bool clear_boot_memory) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  uint64_t flags = platform_boot_policy_initial_flags;
  transport.record->auxiliary_state = 0;

  if ((platform_read_effective_production_mode() & 0x100u) != 0) {
    flags |= PLATFORM_BOOT_POLICY_FLAG28;
  } else {
    flags |= PLATFORM_BOOT_POLICY_FLAG4;
  }
  if ((j_platform_read_production_mode_config_0x08() & 0x100u) == 0) {
    flags |= PLATFORM_BOOT_POLICY_FLAG20 | PLATFORM_BOOT_POLICY_FLAG5;
  }
  if (platform_boot_policy_flag27_default_is_set()) {
    flags |= PLATFORM_BOOT_POLICY_FLAG27;
  }
  transport.record->flags = flags;
  platform_boot_policy_memory_bounds_update(0, 0, 0, 0, 0, true);

  if (clear_boot_memory) {
    transport.service_watchdog(transport.context);
    transport.clear_memory(
        transport.context,
        PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE,
        PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE
    );
  }
  platform_boot_policy_memory_bounds_update(
      (const void *)(uintptr_t)PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE,
      (const void *)(uintptr_t)PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE,
      (const void *)(uintptr_t)(PLATFORM_BOOT_POLICY_BOOT_MEMORY_BASE +
                                PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE),
      &platform_boot_policy_byte_type,
      PLATFORM_BOOT_POLICY_BOOT_MEMORY_SIZE,
      false
  );
  return 0;
}

void platform_boot_policy_memory_bounds_update(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t byte_count,
    bool reset_bounds
) {
  (void)lower_bound;
  (void)upper_bound;
  (void)type_context;
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  if ((transport.record->flags & PLATFORM_BOOT_POLICY_FLAG16) != 0) {
    return;
  }
  if (reset_bounds) {
    transport.record->memory_lower_bound = UINT64_MAX;
    transport.record->memory_upper_bound = 0;
    return;
  }
  const uint64_t address = (uint64_t)(uintptr_t)current;
  if (address > UINT64_MAX - byte_count) {
    platform_boot_policy_memory_bounds_overflow_fatal();
  }
  const uint64_t end = address + byte_count;
  if (transport.record->memory_lower_bound > address) {
    transport.record->memory_lower_bound = address;
  }
  if (transport.record->memory_upper_bound < end) {
    transport.record->memory_upper_bound = end;
  }
}

_Noreturn void platform_boot_policy_memory_bounds_overflow_fatal(void) {
  platform_boot_policy_contract_failure();
}

static uint64_t platform_boot_policy_flag5_profile(uint64_t flags) {
  return (flags & platform_boot_policy_flag5_profile_mask) |
         PLATFORM_BOOT_POLICY_FLAG5;
}

bool platform_boot_policy_flags_accept_and_promote_request(
    uint64_t requested_flags) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  uint64_t flags = transport.record->flags;
  bool accepted = (flags & requested_flags) == requested_flags;
  if ((requested_flags & PLATFORM_BOOT_POLICY_FLAG4) != 0 && accepted) {
    flags = platform_boot_policy_flag5_profile(flags);
    transport.record->flags = flags;
    accepted = (flags & requested_flags) == requested_flags;
  }
  return accepted;
}

void platform_boot_policy_select_flag5_profile(void) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  transport.record->flags =
      platform_boot_policy_flag5_profile(transport.record->flags);
}

bool platform_boot_policy_memory_span_allowed(
    uint64_t current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t byte_count
) {
  (void)lower_bound;
  (void)upper_bound;
  (void)type_context;
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  if ((transport.record->flags & PLATFORM_BOOT_POLICY_FLAG16) != 0) {
    return true;
  }
  if (current > UINT64_MAX - byte_count) {
    return false;
  }
  return transport.record->memory_lower_bound <= current &&
         current + byte_count <= transport.record->memory_upper_bound;
}

bool j_platform_boot_policy_memory_span_allowed(
    uint64_t current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t byte_count
) {
  return platform_boot_policy_memory_span_allowed(
      current, lower_bound, upper_bound, type_context, byte_count);
}

void platform_boot_policy_flag24_set(bool enabled) {
  platform_boot_policy_replace_flag(PLATFORM_BOOT_POLICY_FLAG24, enabled);
}

bool platform_boot_policy_flag22_is_set(void) {
  return (platform_boot_policy_transport().record->flags &
          PLATFORM_BOOT_POLICY_FLAG22) != 0;
}

uint16_t platform_boot_policy_compare_production_mode_byte(uint8_t expected) {
  const uint16_t value =
      (uint16_t)j_platform_read_production_mode_config_0x08();
  const uint8_t actual = (uint8_t)(value >> 8);
  const uint16_t mismatch =
      actual != expected && actual != 0 ? UINT16_C(0x100) : 0;
  return (uint16_t)((value & UINT16_C(0xff)) | mismatch);
}

void platform_boot_policy_set_entropy_seed_span(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  if (current == 0) {
    transport.record->flags &= ~PLATFORM_BOOT_POLICY_FLAG26;
    return;
  }
  platform_boot_policy_require_seed_span(
      current,
      lower_bound,
      upper_bound,
      type_context
  );
  transport.record->flags |= PLATFORM_BOOT_POLICY_FLAG26;
  const uint8_t *source = current;
  for (size_t index = 0; index < PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE;
       ++index) {
    transport.record->entropy_seed[index] = source[index];
  }
}

void platform_boot_policy_set_entropy_context_word(uint32_t context) {
  *platform_boot_policy_transport().entropy_context = context;
}

void platform_boot_policy_flag25_set(bool enabled) {
  platform_boot_policy_replace_flag(PLATFORM_BOOT_POLICY_FLAG25, enabled);
}

void platform_boot_policy_apply_entropy_identity_state(void) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  const volatile PlatformBootPolicyRecord *record = transport.record;

  /* The entropy-policy owner publishes seed provenance and the
   * bit-25 policy result through the platform identity block consumed by
   * the next-stage bootloader. */
  if ((record->flags & PLATFORM_BOOT_POLICY_FLAG26) != 0) {
    const uint32_t *words =
        (const uint32_t *)(const void *)record->entropy_seed;
    (void)platform_identity_set_boot_hardware_words(
        words,
        words,
        words + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    );
  } else {
    (void)platform_identity_set_boot_hardware_words(0, 0, 0, 0);
  }
  platform_status_flag3_update((record->flags & PLATFORM_BOOT_POLICY_FLAG25) !=
                               0);
}

int platform_boot_policy_publish_entropy_request(void) {
  const PlatformBootPolicyTransport transport =
      platform_boot_policy_transport();
  const volatile PlatformBootPolicyRecord *record = transport.record;
  uint8_t request_data[PLATFORM_ENTROPY_REQUEST_DATA_SIZE] = {0};
  if ((record->flags & PLATFORM_BOOT_POLICY_FLAG26) != 0) {
    for (size_t index = 0; index < PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE;
         ++index) {
      request_data[index] = record->entropy_seed[index];
    }
  }
  const uint32_t context = *transport.entropy_context;
  request_data[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE + 0] =
      (uint8_t)context;
  request_data[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE + 1] =
      (uint8_t)(context >> 8);
  request_data[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE + 2] =
      (uint8_t)(context >> 16);
  request_data[PLATFORM_BOOT_POLICY_ENTROPY_SEED_SIZE + 3] =
      (uint8_t)(context >> 24);
  if (platform_entropy_write_request(&platform_entropy_primary_request,
                                     request_data,
                                     sizeof(request_data)) != 0) {
    return -1;
  }
  if (platform_entropy_enable_request(&platform_entropy_primary_request) !=
      0) {
    return -1;
  }
  platform_entropy_materialize_primary_handoff(
      request_data, PLATFORM_ENTROPY_HANDOFF_DATA_SIZE);
  return 0;
}

void platform_boot_policy_flag27_set(void) {
  platform_boot_policy_transport().record->flags |= PLATFORM_BOOT_POLICY_FLAG27;
}

void platform_boot_policy_flag30_set(void) {
  platform_boot_policy_transport().record->flags |= PLATFORM_BOOT_POLICY_FLAG30;
}

void platform_boot_policy_flag42_set(void) {
  platform_boot_policy_transport().record->flags |= PLATFORM_BOOT_POLICY_FLAG42;
}

bool platform_boot_policy_flag40_is_clear(void) {
  return (platform_boot_policy_transport().record->flags &
          PLATFORM_BOOT_POLICY_FLAG40) == 0;
}

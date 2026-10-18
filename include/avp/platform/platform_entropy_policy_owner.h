#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/platform/platform_entropy.h"
#include "avp/platform/platform_entropy_transport_owner.h"
#include "avp/runtime/firebloom/runtime_firebloom.h"


#if __has_attribute(retain)
#define PLATFORM_ENTROPY_POLICY_PUBLIC __attribute__((used, retain))
#else
#define PLATFORM_ENTROPY_POLICY_PUBLIC __attribute__((used))
#endif

enum {
    PLATFORM_ENTROPY_POLICY_FLAG24 = UINT64_C(1) << 24,
    PLATFORM_ENTROPY_POLICY_FLAG31 = UINT64_C(1) << 31,
    PLATFORM_ENTROPY_REQUEST_STATUS_ADDRESS = 0x006000d4,
};

typedef struct {
    bool initialized;
    uint8_t reserved[7];
    uint64_t boot_policy_flags;
    uint16_t production_mode;
    uint16_t reserved_12;
    uint32_t seed_context;
    bool seed_present;
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE];
} PlatformEntropyPolicyState;

typedef struct {
    PlatformEntropy *entropy;
    PlatformEntropyPolicyState *policy;
} PlatformEntropyPolicyBinding;

typedef struct {
    uintptr_t current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    const RuntimeFirebloomType *type;
} PlatformEntropyQwordTuple;

typedef struct {
    size_t byte_offset;
    size_t word_index;
    uintptr_t register_base;
} PlatformEntropyWriteFallbackState;

typedef struct {
    PlatformEntropyRequestRecord request;
    uintptr_t stack_guard;
} PlatformEntropySavedRequest;

extern const RuntimeFirebloomType platform_entropy_policy_byte_type;
extern const RuntimeFirebloomType platform_entropy_policy_qword_type;

void platform_entropy_policy_bind(PlatformEntropyPolicyBinding binding);
void platform_entropy_policy_reset(void);

PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyStatus
platform_entropy_policy_initialize(void);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyStatus
platform_entropy_enable_primary_request_thunk(void);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyStatus
platform_entropy_seed_primary_data_thunk(
    const uint8_t *seed,
    size_t seed_size,
    uint32_t context
);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyQwordTuple
platform_entropy_outlined_validate_saved_qword_tuple_x19_x22(
    PlatformEntropyQwordTuple tuple
);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyQwordTuple
platform_entropy_outlined_validate_saved_qword_tuple_x19_x23(
    PlatformEntropyQwordTuple tuple
);
PLATFORM_ENTROPY_POLICY_PUBLIC RuntimeFirebloomSpan
platform_entropy_outlined_prepare_stack_seed_span(
    uint8_t seed[PLATFORM_ENTROPY_REQUEST_DATA_SIZE]
);
PLATFORM_ENTROPY_POLICY_PUBLIC uintptr_t
platform_entropy_outlined_load_request_status_register_to_w8(void);
PLATFORM_ENTROPY_POLICY_PUBLIC const PlatformEntropyRequestRecord *
platform_entropy_outlined_load_request_type_to_x3(void);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyRequestRecord
platform_entropy_outlined_copy_primary_request_identifier_to_stack(void);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyWriteFallbackState
platform_entropy_outlined_clear_write_fallback_state(void);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropySavedRequest
platform_entropy_outlined_save_request_tuple_and_stack_guard(
    const PlatformEntropyRequestRecord *request,
    uintptr_t stack_guard
);
PLATFORM_ENTROPY_POLICY_PUBLIC void
platform_entropy_outlined_control_flag_frame_epilogue(void);
PLATFORM_ENTROPY_POLICY_PUBLIC void
platform_entropy_outlined_control_flag_frame_prologue(void);
PLATFORM_ENTROPY_POLICY_PUBLIC void
platform_entropy_derive_boot_policy_outputs(
    bool *primary,
    bool *secondary
);
PLATFORM_ENTROPY_POLICY_PUBLIC PlatformEntropyStatus
platform_entropy_seed_and_enable_primary_request(void);
PLATFORM_ENTROPY_POLICY_PUBLIC RuntimeFirebloomSpan
platform_entropy_return_empty_seed_span(void);
PLATFORM_ENTROPY_POLICY_PUBLIC RuntimeFirebloomSpan
platform_entropy_outlined_prepare_stack_seed_span_sp8_x20(
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
);
PLATFORM_ENTROPY_POLICY_PUBLIC bool
platform_entropy_stack_guard_compare_x8(uintptr_t saved_guard);
PLATFORM_ENTROPY_POLICY_PUBLIC uintptr_t
platform_entropy_stack_guard_load_x8(void);
PLATFORM_ENTROPY_POLICY_PUBLIC const RuntimeFirebloomType *
platform_outlined_load_byte_type_to_x3(void);

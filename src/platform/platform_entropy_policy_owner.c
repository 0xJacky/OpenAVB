#include "avp/platform/platform_entropy_policy_owner.h"

#include "avp/runtime/memory/runtime_memory.h"


extern uintptr_t __stack_chk_guard;

const RuntimeFirebloomType platform_entropy_policy_byte_type = {
    .identity = 0x00144f18,
    .element_size = 1,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType platform_entropy_policy_qword_type = {
    .identity = 0x00144f00,
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = true,
};

#if defined(__ELF__)
#define PLATFORM_ENTROPY_POLICY_STATE \
    __attribute__((section(".runtime_prng.state"), used))
#else
#define PLATFORM_ENTROPY_POLICY_STATE
#endif

static PlatformEntropyPolicyBinding bound_binding
    PLATFORM_ENTROPY_POLICY_STATE;

static _Noreturn void platform_entropy_policy_fatal(void) {
    runtime_firebloom_panic();
}

static PlatformEntropyPolicyBinding platform_entropy_policy_binding(void) {
    if (bound_binding.entropy == 0 || bound_binding.policy == 0) {
        platform_entropy_policy_fatal();
    }
    return bound_binding;
}

void platform_entropy_policy_bind(PlatformEntropyPolicyBinding binding) {
    if (binding.entropy == 0 || binding.policy == 0) {
        platform_entropy_policy_fatal();
    }
    bound_binding = binding;
}

void platform_entropy_policy_reset(void) {
    bound_binding = (PlatformEntropyPolicyBinding){0};
}

PlatformEntropyStatus platform_entropy_policy_initialize(void) {
    PlatformEntropyPolicyBinding binding = platform_entropy_policy_binding();
    if (binding.policy->initialized) {
        platform_entropy_reinitialize_fatal();
    }
    binding.policy->initialized = true;
    return platform_entropy_seed_and_enable_primary_request();
}

PlatformEntropyStatus platform_entropy_enable_primary_request_thunk(void) {
    PlatformEntropyPolicyBinding binding = platform_entropy_policy_binding();
    return platform_entropy_enable_primary_request(binding.entropy);
}

PlatformEntropyStatus platform_entropy_seed_primary_data_thunk(
    const uint8_t *seed,
    size_t seed_size,
    uint32_t context
) {
    PlatformEntropyPolicyBinding binding = platform_entropy_policy_binding();
    if (seed == 0 || seed_size < PLATFORM_ENTROPY_SEED_SIZE) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    binding.policy->seed_context = context;
    return platform_entropy_seed_primary_data(binding.entropy, seed);
}

static PlatformEntropyQwordTuple platform_entropy_validate_qword_tuple(
    PlatformEntropyQwordTuple tuple
) {
    if (tuple.type != &platform_entropy_policy_qword_type ||
        tuple.current < tuple.lower_bound || tuple.current > tuple.upper_bound ||
        sizeof(uint64_t) > tuple.upper_bound - tuple.current) {
        platform_entropy_policy_fatal();
    }
    return tuple;
}

PlatformEntropyQwordTuple
platform_entropy_outlined_validate_saved_qword_tuple_x19_x22(
    PlatformEntropyQwordTuple tuple
) {
    return platform_entropy_validate_qword_tuple(tuple);
}

PlatformEntropyQwordTuple
platform_entropy_outlined_validate_saved_qword_tuple_x19_x23(
    PlatformEntropyQwordTuple tuple
) {
    return platform_entropy_validate_qword_tuple(tuple);
}

RuntimeFirebloomSpan platform_entropy_outlined_prepare_stack_seed_span(
    uint8_t seed[PLATFORM_ENTROPY_REQUEST_DATA_SIZE]
) {
    return (RuntimeFirebloomSpan){
        .current = seed,
        .lower_bound = seed,
        .upper_bound = seed + PLATFORM_ENTROPY_REQUEST_DATA_SIZE,
        .type = &platform_entropy_policy_byte_type,
    };
}

uintptr_t platform_entropy_outlined_load_request_status_register_to_w8(void) {
    return PLATFORM_ENTROPY_REQUEST_STATUS_ADDRESS;
}

const PlatformEntropyRequestRecord *
platform_entropy_outlined_load_request_type_to_x3(void) {
    return &platform_entropy_primary_request;
}

PlatformEntropyRequestRecord
platform_entropy_outlined_copy_primary_request_identifier_to_stack(void) {
    return platform_entropy_primary_request;
}

PlatformEntropyWriteFallbackState
platform_entropy_outlined_clear_write_fallback_state(void) {
    return (PlatformEntropyWriteFallbackState){
        .byte_offset = 0,
        .word_index = 0,
        .register_base = 0x00600090,
    };
}

PlatformEntropySavedRequest
platform_entropy_outlined_save_request_tuple_and_stack_guard(
    const PlatformEntropyRequestRecord *request,
    uintptr_t stack_guard
) {
    if (request == 0) {
        platform_entropy_policy_fatal();
    }
    return (PlatformEntropySavedRequest){
        .request = *request,
        .stack_guard = stack_guard,
    };
}

void platform_entropy_outlined_control_flag_frame_epilogue(void) {
}

void platform_entropy_outlined_control_flag_frame_prologue(void) {
}

void platform_entropy_derive_boot_policy_outputs(
    bool *primary,
    bool *secondary
) {
    if (primary == 0 || secondary == 0) {
        platform_entropy_policy_fatal();
    }
    PlatformEntropyPolicyBinding binding = platform_entropy_policy_binding();
    const PlatformEntropyPolicyState *policy = binding.policy;
    const bool permitted =
        (policy->boot_policy_flags & PLATFORM_ENTROPY_POLICY_FLAG31) == 0;
    const bool production = (policy->production_mode & 0x100u) != 0;
    *secondary = permitted;
    *primary = permitted && production &&
        (policy->initialized ||
         (policy->boot_policy_flags & PLATFORM_ENTROPY_POLICY_FLAG24) == 0);
}

PlatformEntropyStatus platform_entropy_seed_and_enable_primary_request(void) {
    PlatformEntropyPolicyBinding binding = platform_entropy_policy_binding();
    uint8_t empty_seed[PLATFORM_ENTROPY_SEED_SIZE] = {0};
    const uint8_t *seed = binding.policy->seed_present ?
        binding.policy->seed : empty_seed;
    PlatformEntropyStatus status = platform_entropy_seed_primary_data_thunk(
        seed,
        PLATFORM_ENTROPY_SEED_SIZE,
        binding.policy->seed_context);
    if (status != PLATFORM_ENTROPY_STATUS_OK) {
        return status;
    }
    return platform_entropy_enable_primary_request_thunk();
}

RuntimeFirebloomSpan platform_entropy_return_empty_seed_span(void) {
    return (RuntimeFirebloomSpan){0};
}

RuntimeFirebloomSpan
platform_entropy_outlined_prepare_stack_seed_span_sp8_x20(
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
) {
    return (RuntimeFirebloomSpan){
        .current = seed,
        .lower_bound = seed,
        .upper_bound = seed + PLATFORM_ENTROPY_SEED_SIZE,
        .type = &platform_entropy_policy_byte_type,
    };
}

bool platform_entropy_stack_guard_compare_x8(uintptr_t saved_guard) {
    return saved_guard == __stack_chk_guard;
}

uintptr_t platform_entropy_stack_guard_load_x8(void) {
    return __stack_chk_guard;
}

const RuntimeFirebloomType *platform_outlined_load_byte_type_to_x3(void) {
    return &platform_entropy_policy_byte_type;
}

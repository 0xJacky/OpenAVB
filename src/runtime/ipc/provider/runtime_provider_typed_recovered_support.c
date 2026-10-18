#include "runtime_provider_typed_recovered_private.h"

enum {
    RUNTIME_PROVIDER_BYTE_TYPE_IDENTITY = 0x62797465,
};

static const RuntimeFirebloomType runtime_provider_byte_type = {
    .identity = RUNTIME_PROVIDER_BYTE_TYPE_IDENTITY,
    .element_size = 1,
    .bytewise_copyable = true,
};

void runtime_provider_register_typed_storage(
    AVPRuntimeProvider *provider,
    uint64_t owner_domain,
    const char *lookup_key,
    const char *metadata_a,
    const char *metadata_b,
    RuntimeProviderTypedStorage *storage
) {
    storage = runtime_provider_typed_require_storage(storage);
    runtime_provider_register(
        provider,
        owner_domain,
        lookup_key,
        metadata_a,
        metadata_b,
        storage,
        (uintptr_t)storage,
        (uintptr_t)storage + sizeof(*storage),
        0
    );
}

void *runtime_provider_validate_common_tuple(
    RuntimeProviderObjectView view,
    size_t required_size
) {
    const uintptr_t current = (uintptr_t)view.current;
    const uintptr_t lower = (uintptr_t)view.lower_bound;
    const uintptr_t upper = (uintptr_t)view.upper_bound;
    if (view.current == 0 || current < lower || current >= upper ||
        required_size > upper - current) {
        runtime_provider_typed_fail();
    }
    return view.current;
}

#define RUNTIME_PROVIDER_DEFINE_VALIDATOR(name)                                \
    void *name(RuntimeProviderObjectView view, size_t required_size) {         \
        return runtime_provider_validate_common_tuple(view, required_size);    \
    }
RUNTIME_PROVIDER_DEFINE_VALIDATOR(runtime_provider_validate_descriptor_tuple)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(runtime_provider_validate_dispatch_tuple)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(
    runtime_provider_validate_dispatch_tuple_clone)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(
    runtime_provider_validate_implementation_tuple)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(runtime_provider_validate_tuple_tail_clone)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(
    runtime_provider_validate_registration_entry_tuple)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(
    runtime_provider_validate_byte_tuple_tail_clone_1)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(
    runtime_provider_validate_byte_tuple_tail_clone_2)
RUNTIME_PROVIDER_DEFINE_VALIDATOR(runtime_provider_common_tuple_validate_clone)
#undef RUNTIME_PROVIDER_DEFINE_VALIDATOR

void runtime_provider_store_source_location(
    RuntimeProviderSourceLocation *destination,
    uint64_t token,
    uint64_t offset
) {
    if (destination == 0) {
        runtime_provider_typed_fail();
    }
    *destination = (RuntimeProviderSourceLocation){
        .token = token,
        .offset = offset,
    };
}

RuntimeProviderSourceLocation
runtime_provider_log_source_offset(uint64_t offset) {
    return (RuntimeProviderSourceLocation){
        .token = runtime_provider_storage_normalization_source_token(),
        .offset = offset,
    };
}

RuntimeProviderSourceLocation
runtime_provider_log_source_location(RuntimeProviderSourceLocation location) {
    return location;
}

_Noreturn void runtime_provider_assert_failure(void) {
    runtime_provider_typed_fail();
}

RuntimeProviderRouteValue
runtime_provider_copy_16_byte_route_value(RuntimeProviderRouteValue route) {
    return route;
}

RuntimeProviderRouteValue runtime_provider_copy_16_byte_route_value_clone(
    RuntimeProviderRouteValue route) {
    return route;
}

RuntimeProviderRecoveredValue runtime_provider_return_empty_common_tuple(void) {
    return (RuntimeProviderRecoveredValue){0};
}

uint32_t runtime_provider_outlined_make_low_bits_mask_w8(uint32_t bit_count) {
    const uint32_t shift = bit_count & 31;
    return shift == 0 ? 0 : (UINT32_C(1) << shift) - 1;
}

size_t runtime_firebloom_prepare_16_byte_count_clone(void) { return 16; }

const RuntimeFirebloomType *runtime_firebloom_load_byte_type_to_x7_clone(void) {
    return &runtime_provider_byte_type;
}

uint64_t runtime_provider_storage_normalization_source_token(void) {
    return UINT64_C(0x048fe3b6e2f469df);
}

#define RUNTIME_PROVIDER_DEFINE_VALUE_RESTORE(name)                            \
    RuntimeProviderRecoveredValue name(RuntimeProviderRecoveredValue value) {  \
        return value;                                                          \
    }
RUNTIME_PROVIDER_DEFINE_VALUE_RESTORE(runtime_provider_restore_traversal_tuple)
RUNTIME_PROVIDER_DEFINE_VALUE_RESTORE(
    runtime_provider_restore_destination_tuple)
RUNTIME_PROVIDER_DEFINE_VALUE_RESTORE(runtime_provider_restore_nested_tuple)
#undef RUNTIME_PROVIDER_DEFINE_VALUE_RESTORE

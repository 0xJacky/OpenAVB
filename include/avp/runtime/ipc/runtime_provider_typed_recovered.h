#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/ipc/runtime_provider.h"

typedef struct RuntimeProviderTypedDescriptor RuntimeProviderTypedDescriptor;
typedef struct RuntimeProviderTypedImplementation
    RuntimeProviderTypedImplementation;
typedef struct RuntimeProviderTypedStorage RuntimeProviderTypedStorage;
typedef struct RuntimeProviderStorageOperation RuntimeProviderStorageOperation;

typedef bool (*RuntimeProviderTypedCallback)(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
typedef bool (*RuntimeProviderRouteCall)(
    void *context,
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
);

typedef struct {
    RuntimeProviderRouteCall call;
    void *context;
} RuntimeProviderRouteValue;

_Static_assert(sizeof(RuntimeProviderRouteValue) == 16,
               "provider route values preserve the recovered 16-byte ABI");

typedef enum {
    RUNTIME_PROVIDER_OPERATION_COMPARE = 0,
    RUNTIME_PROVIDER_OPERATION_READ = 1,
    RUNTIME_PROVIDER_OPERATION_WRITE = 2,
    RUNTIME_PROVIDER_OPERATION_CONTROL = 4,
} RuntimeProviderOperationKind;

struct RuntimeProviderStorageOperation {
    RuntimeProviderOperationKind kind;
    uint32_t element_offset;
    uint32_t element_count;
    uint8_t *buffer;
    size_t buffer_size;
    size_t byte_offset;
    size_t byte_count;
    size_t transferred;
    int status;
};

struct RuntimeProviderTypedDescriptor {
    size_t instance_size;
    size_t alignment;
    uint32_t alignment_shift;
    bool enabled;
    RuntimeProviderTypedImplementation *implementation;
};

struct RuntimeProviderTypedImplementation {
    RuntimeProviderTypedDescriptor *descriptor;
    void *context;
    RuntimeProviderTypedCallback callbacks[5];
    RuntimeProviderRouteValue read_route;
    RuntimeProviderRouteValue write_route;
    size_t alignment;
    uint32_t alignment_shift;
    uint32_t references;
    bool enabled;
};

struct RuntimeProviderTypedStorage {
    RuntimeProviderTypedDescriptor *descriptor;
    RuntimeProviderTypedImplementation *implementation;
    uint8_t *bytes;
    size_t byte_size;
    uint32_t element_shift;
};

typedef struct {
    void *current;
    void *lower_bound;
    void *upper_bound;
} RuntimeProviderObjectView;

typedef struct {
    uintptr_t words[8];
} RuntimeProviderRecoveredValue;

typedef struct {
    uint64_t token;
    uint64_t offset;
} RuntimeProviderSourceLocation;

#define RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC                                \
    __attribute__((used, retain, minsize, noinline))

RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_firebloom_descriptor_implementation_dispatch(
    RuntimeProviderTypedDescriptor *descriptor,
    unsigned slot,
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_firebloom_descriptor_implementation_is_enabled(
    const RuntimeProviderTypedDescriptor *descriptor);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_register_typed_storage(
    AVPRuntimeProvider *provider,
    uint64_t owner_domain,
    const char *lookup_key,
    const char *metadata_a,
    const char *metadata_b,
    RuntimeProviderTypedStorage *storage
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot0_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_operation_dispatch(
    RuntimeProviderTypedStorage *storage,
    unsigned slot,
    RuntimeProviderStorageOperation *operation
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot1_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_read_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_write_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot4_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_set_descriptor_alignment_within_instance_size(
    RuntimeProviderTypedDescriptor *descriptor,
    size_t alignment
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_set_descriptor_alignment_above_instance_size(
    RuntimeProviderTypedDescriptor *descriptor,
    size_t alignment
);

#define RUNTIME_PROVIDER_DECLARE_BIND_SLOT(number)                             \
    RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void                               \
    runtime_provider_bind_callback_slot##number(                               \
        RuntimeProviderTypedImplementation *implementation,                    \
        RuntimeProviderTypedCallback callback)
RUNTIME_PROVIDER_DECLARE_BIND_SLOT(0);
RUNTIME_PROVIDER_DECLARE_BIND_SLOT(1);
RUNTIME_PROVIDER_DECLARE_BIND_SLOT(2);
RUNTIME_PROVIDER_DECLARE_BIND_SLOT(3);
RUNTIME_PROVIDER_DECLARE_BIND_SLOT(4);
#undef RUNTIME_PROVIDER_DECLARE_BIND_SLOT

RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderTypedImplementation *
runtime_firebloom_descriptor_retain_implementation(
    RuntimeProviderTypedDescriptor *descriptor);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_bind_read_route_dispatch_bridge(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_bind_write_route_dispatch_bridge(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC uint32_t
runtime_provider_alignment_shift(size_t alignment);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void runtime_provider_bind_read_route(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void runtime_provider_bind_write_route(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_compare(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    const void *expected,
    size_t byte_count
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot0_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_read_route(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_write_route(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot1_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_read_route_thunk(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot4_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_set_implementation_alignment_within_instance_size(
    RuntimeProviderTypedImplementation *implementation,
    size_t alignment
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_set_implementation_alignment_above_instance_size(
    RuntimeProviderTypedImplementation *implementation,
    size_t alignment
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderTypedImplementation *
runtime_provider_create_implementation(
    RuntimeProviderTypedDescriptor *descriptor,
    RuntimeProviderTypedImplementation *implementation,
    void *context
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_implementation_is_enabled(
    const RuntimeProviderTypedImplementation *implementation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderTypedImplementation *
runtime_provider_resolve_implementation_dispatch_tuple(
    RuntimeProviderTypedDescriptor *descriptor);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_read(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    void *destination,
    size_t byte_count
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot1_unsupported(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_write(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    const void *source,
    size_t byte_count
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot3_unsupported(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot4_unsupported(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_typed_storage_slot5_unsupported(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void
runtime_provider_store_source_location(
    RuntimeProviderSourceLocation *destination,
    uint64_t token,
    uint64_t offset
);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC _Noreturn void
runtime_provider_assert_failure(void);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRouteValue
runtime_provider_copy_16_byte_route_value(RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRouteValue
runtime_provider_copy_16_byte_route_value_clone(
    RuntimeProviderRouteValue route);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRecoveredValue
runtime_provider_return_empty_common_tuple(void);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC uint32_t
runtime_provider_outlined_make_low_bits_mask_w8(uint32_t bit_count);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderSourceLocation
runtime_provider_log_source_offset(uint64_t offset);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderSourceLocation
runtime_provider_log_source_location(RuntimeProviderSourceLocation location);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC bool
runtime_provider_normalize_storage_operation_record(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC uint64_t
runtime_provider_storage_normalization_source_token(void);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC size_t
runtime_firebloom_prepare_16_byte_count_clone(void);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC const RuntimeFirebloomType *
runtime_firebloom_load_byte_type_to_x7_clone(void);

#define RUNTIME_PROVIDER_DECLARE_VALIDATOR(name)                               \
    RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC void *name(                        \
        RuntimeProviderObjectView view, size_t required_size)
RUNTIME_PROVIDER_DECLARE_VALIDATOR(runtime_provider_validate_descriptor_tuple);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(runtime_provider_validate_dispatch_tuple);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_validate_dispatch_tuple_clone);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_validate_implementation_tuple);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(runtime_provider_validate_tuple_tail_clone);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(runtime_provider_validate_common_tuple);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_validate_registration_entry_tuple);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_validate_byte_tuple_tail_clone_1);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_validate_byte_tuple_tail_clone_2);
RUNTIME_PROVIDER_DECLARE_VALIDATOR(
    runtime_provider_common_tuple_validate_clone);
#undef RUNTIME_PROVIDER_DECLARE_VALIDATOR

RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRecoveredValue
runtime_provider_restore_traversal_tuple(RuntimeProviderRecoveredValue value);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRecoveredValue
runtime_provider_restore_destination_tuple(RuntimeProviderRecoveredValue value);
RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRecoveredValue
runtime_provider_restore_nested_tuple(RuntimeProviderRecoveredValue value);

#define RUNTIME_PROVIDER_RECOVERED_OUTLINER(name)                              \
    RUNTIME_PROVIDER_TYPED_RECOVERED_PUBLIC RuntimeProviderRecoveredValue      \
    name(RuntimeProviderRecoveredValue value);
#include "avp/runtime/ipc/runtime_provider_recovered_outliners.inc"
#undef RUNTIME_PROVIDER_RECOVERED_OUTLINER

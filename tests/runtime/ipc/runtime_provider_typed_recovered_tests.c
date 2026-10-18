#include "avp/runtime/ipc/runtime_provider_typed_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    unsigned callbacks;
    unsigned routes;
} TestContext;

static bool test_callback(RuntimeProviderTypedImplementation *implementation,
                          RuntimeProviderTypedStorage *storage,
                          RuntimeProviderStorageOperation *operation) {
    TestContext *context = implementation->context;
    ++context->callbacks;
    return runtime_provider_typed_storage_read_route(storage, operation);
}

static bool test_route(void *opaque, RuntimeProviderTypedStorage *storage,
                       RuntimeProviderStorageOperation *operation) {
    TestContext *context = opaque;
    ++context->routes;
    for (size_t index = 0; index < operation->byte_count; ++index) {
        operation->buffer[index] =
            storage->bytes[operation->byte_offset + index];
    }
    operation->transferred = operation->byte_count;
    operation->status = 0;
    return true;
}

static bool recovered_values_equal(RuntimeProviderRecoveredValue left,
                                   RuntimeProviderRecoveredValue right) {
    for (size_t index = 0; index < 8; ++index) {
        if (left.words[index] != right.words[index]) {
            return false;
        }
    }
    return true;
}

static void test_descriptor_and_storage(void) {
    TestContext context = {0};
    RuntimeProviderTypedDescriptor descriptor = {
        .instance_size = 64,
    };
    runtime_provider_set_descriptor_alignment_within_instance_size(&descriptor,
                                                                   8);
    assert(descriptor.alignment_shift == 3);
    runtime_provider_set_descriptor_alignment_above_instance_size(&descriptor,
                                                                  128);
    assert(descriptor.alignment_shift == 7);

    RuntimeProviderTypedImplementation implementation = {0};
    assert(runtime_provider_create_implementation(&descriptor, &implementation,
                                                  &context) == &implementation);
    assert(runtime_provider_implementation_is_enabled(&implementation));
    assert(runtime_firebloom_descriptor_implementation_is_enabled(&descriptor));
    assert(runtime_provider_resolve_implementation_dispatch_tuple(
               &descriptor) == &implementation);
    assert(runtime_firebloom_descriptor_retain_implementation(&descriptor) ==
           &implementation);
    assert(implementation.references == 1);

    runtime_provider_set_implementation_alignment_within_instance_size(
        &implementation, 16);
    assert(implementation.alignment_shift == 4);
    runtime_provider_set_implementation_alignment_above_instance_size(
        &implementation, 256);
    assert(implementation.alignment_shift == 8);

    runtime_provider_bind_callback_slot0(&implementation, test_callback);
    runtime_provider_bind_callback_slot1(&implementation, test_callback);
    runtime_provider_bind_callback_slot2(&implementation, test_callback);
    runtime_provider_bind_callback_slot3(&implementation, test_callback);
    runtime_provider_bind_callback_slot4(&implementation, test_callback);

    uint8_t bytes[32] = {0};
    RuntimeProviderTypedStorage storage = {
        .descriptor = &descriptor,
        .implementation = &implementation,
        .bytes = bytes,
        .byte_size = sizeof(bytes),
        .element_shift = 2,
    };
    const uint8_t source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t destination[8] = {0};
    assert(runtime_provider_typed_storage_write(&storage, 8, source,
                                                sizeof(source)));
    assert(runtime_provider_typed_storage_compare(&storage, 8, source,
                                                  sizeof(source)));
    assert(runtime_provider_typed_storage_read(&storage, 8, destination,
                                               sizeof(destination)));
    for (size_t index = 0; index < sizeof(source); ++index) {
        assert(destination[index] == source[index]);
    }

    RuntimeProviderStorageOperation operation = {
        .kind = RUNTIME_PROVIDER_OPERATION_READ,
        .element_offset = 2,
        .element_count = 2,
        .buffer = destination,
        .buffer_size = sizeof(destination),
    };
    assert(runtime_provider_normalize_storage_operation_record(&storage,
                                                               &operation));
    assert(operation.byte_offset == 8 && operation.byte_count == 8);
    assert(runtime_provider_typed_storage_slot0_invoke(&storage, &operation));
    assert(runtime_provider_typed_storage_slot1_invoke(&storage, &operation));
    assert(runtime_provider_typed_storage_slot4_invoke(&storage, &operation));
    assert(context.callbacks == 3);

    RuntimeProviderRouteValue route = {
        .call = test_route,
        .context = &context,
    };
    runtime_provider_bind_read_route(&implementation, route);
    runtime_provider_bind_write_route(&implementation, route);
    runtime_provider_bind_read_route_dispatch_bridge(&implementation, route);
    runtime_provider_bind_write_route_dispatch_bridge(&implementation, route);
    assert(runtime_provider_typed_storage_read_dispatch_bridge(&storage,
                                                               &operation));
    assert(
        runtime_provider_typed_storage_read_route_thunk(&storage, &operation));
    assert(context.routes == 2);

    assert(runtime_provider_typed_storage_slot0_dispatch_bridge(&storage,
                                                                &operation));
    assert(runtime_provider_typed_storage_slot1_dispatch_bridge(&storage,
                                                                &operation));
    assert(runtime_provider_typed_storage_slot4_dispatch_bridge(&storage,
                                                                &operation));
    assert(runtime_firebloom_descriptor_implementation_dispatch(
        &descriptor, 0, &storage, &operation));

    assert(!runtime_provider_typed_storage_slot1_unsupported(&storage,
                                                             &operation));
    assert(!runtime_provider_typed_storage_slot3_unsupported(&storage,
                                                             &operation));
    assert(!runtime_provider_typed_storage_slot4_unsupported(&storage,
                                                             &operation));
    assert(!runtime_provider_typed_storage_slot5_unsupported(&storage,
                                                             &operation));
}

static void test_registration_and_helpers(void) {
    runtime_provider_registry_initialize();
    RuntimeProviderTypedDescriptor descriptor = {
        .instance_size = 16,
        .alignment = 8,
        .alignment_shift = 3,
    };
    RuntimeProviderTypedImplementation implementation = {0};
    (void)runtime_provider_create_implementation(&descriptor, &implementation,
                                                 0);
    uint8_t bytes[16] = {0};
    RuntimeProviderTypedStorage storage = {
        .descriptor = &descriptor,
        .implementation = &implementation,
        .bytes = bytes,
        .byte_size = sizeof(bytes),
    };
    AVPRuntimeProvider provider = {0};
    runtime_provider_register_typed_storage(&provider, 3, "typed", "provider",
                                            "storage", &storage);
    assert(runtime_provider_publish(&storage) == 0);
    AVPRuntimeProviderHandle handle = runtime_provider_lookup("typed");
    assert(runtime_provider_resolve(handle) == &storage);
    runtime_provider_release(handle);
    runtime_provider_unpublish(&provider);
    runtime_provider_unregister(&provider);

    uint8_t object[32] = {0};
    RuntimeProviderObjectView view = {
        .current = object + 4,
        .lower_bound = object,
        .upper_bound = object + sizeof(object),
    };
#define TEST_VALIDATOR(name) assert(name(view, 16) == object + 4)
    TEST_VALIDATOR(runtime_provider_validate_descriptor_tuple);
    TEST_VALIDATOR(runtime_provider_validate_dispatch_tuple);
    TEST_VALIDATOR(runtime_provider_validate_dispatch_tuple_clone);
    TEST_VALIDATOR(runtime_provider_validate_implementation_tuple);
    TEST_VALIDATOR(runtime_provider_validate_tuple_tail_clone);
    TEST_VALIDATOR(runtime_provider_validate_common_tuple);
    TEST_VALIDATOR(runtime_provider_validate_registration_entry_tuple);
    TEST_VALIDATOR(runtime_provider_validate_byte_tuple_tail_clone_1);
    TEST_VALIDATOR(runtime_provider_validate_byte_tuple_tail_clone_2);
    TEST_VALIDATOR(runtime_provider_common_tuple_validate_clone);
#undef TEST_VALIDATOR

    RuntimeProviderSourceLocation location = {0};
    runtime_provider_store_source_location(&location, 7, 9);
    assert(location.token == 7 && location.offset == 9);
    location = runtime_provider_log_source_offset(11);
    assert(location.token ==
           runtime_provider_storage_normalization_source_token());
    assert(runtime_provider_log_source_location(location).offset == 11);
    assert(runtime_firebloom_prepare_16_byte_count_clone() == 16);
    assert(runtime_firebloom_load_byte_type_to_x7_clone()->element_size == 1);
    assert(runtime_provider_outlined_make_low_bits_mask_w8(5) == 31);
    assert(runtime_provider_outlined_make_low_bits_mask_w8(32) == 0);

    RuntimeProviderRouteValue route = {.call = test_route, .context = object};
    assert(runtime_provider_copy_16_byte_route_value(route).call == test_route);
    assert(runtime_provider_copy_16_byte_route_value_clone(route).context ==
           object);
    RuntimeProviderRecoveredValue empty =
        runtime_provider_return_empty_common_tuple();
    for (size_t index = 0; index < 8; ++index) {
        assert(empty.words[index] == 0);
    }
}

static void test_recovered_tuple_outliners(void) {
    RuntimeProviderRecoveredValue value = {
        .words = {1, 2, 3, 4, 5, 6, 7, 8},
    };
    assert(recovered_values_equal(
        runtime_provider_restore_traversal_tuple(value), value));
    assert(recovered_values_equal(
        runtime_provider_restore_destination_tuple(value), value));
    assert(recovered_values_equal(runtime_provider_restore_nested_tuple(value),
                                  value));
#define RUNTIME_PROVIDER_RECOVERED_OUTLINER(name)                              \
    assert(recovered_values_equal(name(value), value));
#include "avp/runtime/ipc/runtime_provider_recovered_outliners.inc"
#undef RUNTIME_PROVIDER_RECOVERED_OUTLINER
}

int main(void) {
    test_descriptor_and_storage();
    test_registration_and_helpers();
    test_recovered_tuple_outliners();
    puts("runtime_provider_typed_recovered_tests: ok");
    return 0;
}

#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/support/runtime_diagnostics.h"

#include <assert.h>
#include <setjmp.h>


static uint8_t current_domain = 1;
static jmp_buf fatal_environment;
static uint64_t fatal_source_token;
static uint32_t fatal_line;

_Noreturn void runtime_fatal_at(uint64_t source_token, uint32_t line) {
    fatal_source_token = source_token;
    fatal_line = line;
    longjmp(fatal_environment, 1);
}

static const RuntimeFirebloomType test_object_type = {
    .identity = UINT64_C(0x746573746f626a),
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = false,
};

uint8_t runtime_current_domain(void) {
    return current_domain;
}

int main(void) {
    assert(runtime_capability_address_is_canonical(UINT64_C(0)));
    assert(runtime_capability_address_is_canonical(
        (UINT64_C(1) << 42) - 1
    ));
    assert(runtime_capability_address_is_canonical(UINT64_MAX));
    assert(!runtime_capability_address_is_canonical(UINT64_C(1) << 42));
    assert(!runtime_capability_address_is_canonical(
        UINT64_C(0x155555) << 42
    ));
    runtime_capability_validate_address(UINT64_C(0x7003cad0));
    if (setjmp(fatal_environment) == 0) {
        runtime_capability_validate_address(UINT64_C(1) << 42);
        assert(false);
    }
    assert(fatal_source_token == UINT64_C(0x01e87eca99c7dc29));
    assert(fatal_line == UINT32_C(0x15e));

    uint64_t tuple[4] = {1, 2, 3, 4};
    runtime_capability_clear_tuple(tuple);
    assert(tuple[0] == 0);
    assert(tuple[1] == 0);
    assert(tuple[2] == 0);
    assert(tuple[3] == 0);

    runtime_capability_initialize();
    assert(RUNTIME_OBJECT_CAPACITY == 512);
    assert(RUNTIME_CAPABILITY_CAPACITY == 1024);
    assert(runtime_capability_object_count() == 1);
    assert(runtime_capability_count() == 1);
    assert(runtime_capability_audience_for_domain(1) == UINT16_C(2));
    assert(runtime_capability_audience_for_domain(15) == UINT16_C(0x8000));
    assert(runtime_capability_normalize_audience(
        RUNTIME_CAPABILITY_AUDIENCE_ALL
    ) == UINT16_C(0x7fff));

    uint64_t object = UINT64_C(0x1234);
    const uint16_t handle = runtime_capability_publish(
        &object,
        &test_object_type,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT,
        runtime_capability_audience_for_domain(1)
    );
    assert(handle == 1);
    assert(runtime_capability_object_count() == 2);
    assert(runtime_capability_count() == 2);
    assert(*runtime_capability_entry_lookup(handle) == UINT32_C(0x32000401));
    assert(runtime_capability_object_id(
        *runtime_capability_entry_lookup(handle)
    ) == 1);
    const RuntimeFirebloomSpan resolved_object =
        runtime_object_resolve(1, sizeof(object));
    assert(resolved_object.current == &object);
    assert(resolved_object.lower_bound == &object);
    assert(resolved_object.upper_bound == &object + 1);
    assert(resolved_object.type == &test_object_type);
    assert(runtime_object_registry_span().current != 0);
    assert(runtime_capability_registry_span().current != 0);
    assert(runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) == &object);

    uint8_t domain_object[1] = {0};
    uint8_t event_object[0x48] = {0};
    uint8_t endpoint_object[0x90] = {0};
    uint8_t type6_object[0x90] = {0};
    const RuntimeFirebloomType domain_type = {
        .identity = 2u, .element_size = sizeof(domain_object)};
    const RuntimeFirebloomType event_type = {
        .identity = 5u, .element_size = sizeof(event_object)};
    const RuntimeFirebloomType endpoint_type = {
        .identity = 4u, .element_size = sizeof(endpoint_object)};
    const RuntimeFirebloomType type6_type = {
        .identity = 6u, .element_size = sizeof(type6_object)};
    const uint16_t domain_handle = runtime_capability_publish(
        domain_object, &domain_type, RUNTIME_CAPABILITY_TYPE_DOMAIN,
        RUNTIME_CAPABILITY_RIGHT_WAIT, runtime_capability_audience_for_domain(1));
    const uint16_t event_handle = runtime_capability_publish(
        event_object, &event_type, RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_WAIT, runtime_capability_audience_for_domain(1));
    const uint16_t endpoint_handle = runtime_capability_publish(
        endpoint_object, &endpoint_type,
        RUNTIME_CAPABILITY_TYPE_SERVICE_ENDPOINT,
        RUNTIME_CAPABILITY_RIGHT_WAIT, runtime_capability_audience_for_domain(1));
    const uint16_t type6_handle = runtime_capability_publish(
        type6_object, &type6_type, RUNTIME_CAPABILITY_TYPE_OBJECT,
        RUNTIME_CAPABILITY_RIGHT_WAIT, runtime_capability_audience_for_domain(1));
    assert(runtime_domain_resolve_capability(
               domain_handle, RUNTIME_CAPABILITY_RIGHT_WAIT) == domain_object);
    assert(runtime_event_resolve_capability(
               event_handle, RUNTIME_CAPABILITY_RIGHT_WAIT).current ==
           event_object);
    assert(runtime_endpoint_resolve_capability(
               endpoint_handle, RUNTIME_CAPABILITY_RIGHT_WAIT).current ==
           endpoint_object);
    assert(runtime_type6_resolve_capability(
               type6_handle, RUNTIME_CAPABILITY_RIGHT_WAIT).current ==
           type6_object);
    assert(runtime_event_resolve_capability(
               domain_handle, RUNTIME_CAPABILITY_RIGHT_WAIT).current == NULL);

    const uint16_t merged_handle = runtime_capability_publish(
        &object,
        &test_object_type,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT,
        runtime_capability_audience_for_domain(2)
    );
    assert(merged_handle == handle);
    assert(runtime_capability_object_count() == 6);
    assert(runtime_capability_count() == 6);
    assert(*runtime_capability_entry_lookup(handle) == UINT32_C(0x32000c01));

    current_domain = 2;
    assert(runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) == &object);
    assert(runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_SIGNAL
    ) == 0);

    runtime_capability_initialize();
    current_domain = 1;
    const uint16_t delegating_handle = runtime_capability_publish(
        &object,
        &test_object_type,
        RUNTIME_CAPABILITY_TYPE_TASK,
        UINT8_C(7),
        runtime_capability_audience_for_domain(1)
    );
    const uint16_t derived_handle = runtime_capability_derive(
        delegating_handle,
        RUNTIME_CAPABILITY_RIGHT_WAIT,
        runtime_capability_audience_for_domain(2)
    );
    assert(derived_handle == 2);
    assert(runtime_capability_object_count() == 2);
    assert(runtime_capability_count() == 3);
    assert((*runtime_capability_entry_lookup(derived_handle) &
        UINT32_C(0x0e000000)) == UINT32_C(0x02000000));
    current_domain = 2;
    assert(runtime_capability_resolve(
        derived_handle,
        RUNTIME_CAPABILITY_TYPE_ANY,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) == &object);
    runtime_capability_revoke_rights(
        derived_handle,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    );
    assert((*runtime_capability_entry_lookup(derived_handle) &
        UINT32_C(0x0e000000)) == 0);
    assert(runtime_capability_resolve(
        derived_handle,
        RUNTIME_CAPABILITY_TYPE_TASK,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) == 0);

    runtime_capability_initialize();
    current_domain = 1;
    static uint64_t capacity_objects[511];
    uint16_t object_handles[511];
    for (uint16_t index = 0; index < 511; ++index) {
        object_handles[index] = runtime_capability_publish(
            &capacity_objects[index],
            &test_object_type,
            RUNTIME_CAPABILITY_TYPE_TASK,
            RUNTIME_CAPABILITY_RIGHT_WAIT,
            runtime_capability_audience_for_domain(1)
        );
    }
    assert(runtime_capability_object_count() == RUNTIME_OBJECT_CAPACITY);
    assert(runtime_capability_count() == 512);
    for (uint16_t index = 0; index < 511; ++index) {
        const uint16_t signal_handle = runtime_capability_publish(
            &capacity_objects[index],
            &test_object_type,
            RUNTIME_CAPABILITY_TYPE_TASK,
            RUNTIME_CAPABILITY_RIGHT_SIGNAL,
            runtime_capability_audience_for_domain(1)
        );
        assert(signal_handle == (uint16_t)(object_handles[index] + 511));
    }
    assert(runtime_capability_count() == 1023);
    assert(runtime_capability_publish(
        &capacity_objects[0],
        &test_object_type,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_WAIT,
        runtime_capability_audience_for_domain(1)
    ) == 1023);
    assert(runtime_capability_count() == RUNTIME_CAPABILITY_CAPACITY);
}

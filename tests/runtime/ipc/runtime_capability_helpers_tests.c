#include "avp/runtime/ipc/runtime_capability_helpers.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    uint8_t address_class;
    uintptr_t classified_address;
    size_t classified_size;
    RuntimeFirebloomSpan published_span;
    size_t published_minimum_size;
    uintptr_t validated_address;
    uint16_t resolved_handle;
    uint8_t resolved_type;
    uint8_t resolved_rights;
    uint8_t object[RUNTIME_TYPE6_OBJECT_SIZE];
} MockCapability;

static MockCapability mock;

bool runtime_capability_address_is_canonical(uintptr_t address) {
    const uintptr_t upper = address >> 42;
    return upper == 0 || upper == UINT32_C(0x3fffff);
}

void runtime_capability_validate_address(uintptr_t address) {
    assert(runtime_capability_address_is_canonical(address));
    mock.validated_address = address;
}

uint16_t runtime_object_publish(
    RuntimeFirebloomSpan object,
    size_t minimum_size
) {
    mock.published_span = object;
    mock.published_minimum_size = minimum_size;
    return UINT16_C(0x123);
}

void *runtime_capability_resolve(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights
) {
    mock.resolved_handle = handle;
    mock.resolved_type = type;
    mock.resolved_rights = required_rights;
    return mock.object;
}

static uint8_t mock_classify(
    void *context,
    uintptr_t address,
    size_t size
) {
    MockCapability *capability = context;
    capability->classified_address = address;
    capability->classified_size = size;
    return capability->address_class;
}

static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void invoke_zero_rights_fatal(void) {
    runtime_capability_publish_zero_rights_fatal();
}

static void invoke_invalid_audience_bit(void) {
    (void)runtime_capability_enable_external_audience(16);
}

static void invoke_truncated_type6_tuple(void) {
    (void)runtime_type6_object_tuple_validate((RuntimeFirebloomSpan){
        .current = mock.object,
        .lower_bound = mock.object,
        .upper_bound = mock.object + 0x20,
        .type = &runtime_type6_object_type,
    });
}

static void test_external_audience(void) {
    runtime_capability_helpers_reset_external_audience();
    const RuntimeFirebloomSpan mask =
        runtime_capability_allowed_external_audience_mask_span();
    assert(mask.current == mask.lower_bound);
    assert((uint8_t *)mask.upper_bound - (uint8_t *)mask.current == 2);
    assert(mask.type->element_size == sizeof(uint16_t));
    assert(*(const uint16_t *)mask.current == UINT16_C(0x001e));

    assert(!runtime_capability_enable_external_audience(1));
    assert(runtime_capability_external_audience_bit_is_enabled(1));
    assert(!runtime_capability_enable_external_audience(2));
    assert(!runtime_capability_enable_external_audience(3));
    assert(runtime_capability_enable_external_audience(4));
    assert(runtime_capability_enable_external_audience(4));
    assert(!runtime_capability_external_audience_bit_is_enabled(0));
}

static void test_registry_wrappers(void) {
    const RuntimeFirebloomSpan object = {
        .current = mock.object,
        .lower_bound = mock.object,
        .upper_bound = mock.object + sizeof(mock.object),
        .type = &runtime_type6_object_type,
    };
    assert(runtime_object_publish_32(object) == UINT16_C(0x123));
    assert(mock.published_span.current == mock.object);
    assert(mock.published_minimum_size == 0x20);

    const uintptr_t address = (uintptr_t)mock.object;
    assert(runtime_capability_validate_address_identity(address) == address);
    assert(mock.validated_address == address);

    assert(runtime_type6_resolve_wait_signal_capability_from_x23(
        UINT16_C(0x321)
    ) == mock.object);
    assert(mock.resolved_handle == UINT16_C(0x321));
    assert(mock.resolved_type == RUNTIME_CAPABILITY_TYPE_OBJECT);
    assert(mock.resolved_rights ==
        (RUNTIME_CAPABILITY_RIGHT_WAIT | RUNTIME_CAPABILITY_RIGHT_SIGNAL));
}

static void test_type6_tuple(void) {
    const RuntimeFirebloomType compatible_type = {
        .identity = runtime_type6_object_type.identity,
        .element_size = RUNTIME_TYPE6_OBJECT_SIZE,
        .bytewise_copyable = false,
    };
    const RuntimeFirebloomSpan input = {
        .current = mock.object,
        .lower_bound = mock.object,
        .upper_bound = mock.object + sizeof(mock.object),
        .type = &compatible_type,
    };
    const RuntimeFirebloomSpan result =
        runtime_type6_object_tuple_validate(input);
    assert(result.current == mock.object);
    assert(result.lower_bound == mock.object);
    assert(result.upper_bound == mock.object + sizeof(mock.object));
    assert(result.type == &runtime_type6_object_type);
}

static void test_address_policy(void) {
    const uintptr_t aligned = (uintptr_t)0x70034000;
    mock.address_class = RUNTIME_OBJECT_ADDRESS_CLASS_PLATFORM;
    assert(runtime_object_address_is_admissible(aligned + 1, 0x20));
    assert(mock.classified_address == aligned + 1);
    assert(mock.classified_size == 0x20);

    mock.address_class = RUNTIME_OBJECT_ADDRESS_CLASS_RUNTIME_PRIVATE;
    assert(runtime_object_address_is_admissible(aligned, 0x90));
    assert(!runtime_object_address_is_admissible(aligned + 1, 0x90));

    mock.address_class = RUNTIME_OBJECT_ADDRESS_CLASS_REJECTED;
    assert(!runtime_object_address_is_admissible(aligned, 0x20));
    assert(!runtime_object_address_is_admissible(0, 0x20));
    assert(!runtime_object_address_is_admissible(aligned, 0));
    assert(!runtime_object_address_is_admissible(UINTPTR_MAX - 3, 8));
}

static void test_entry_type(void) {
    const uint32_t mapping = UINT32_C(7) << 28;
    const uint32_t object = UINT32_C(6) << 28;
    assert(runtime_capability_outlined_compare_entry_type_to_7(&mapping));
    assert(!runtime_capability_outlined_compare_entry_type_to_7(&object));
}

int main(void) {
    const RuntimeCapabilityHelpersPlatform platform = {
        .context = &mock,
        .classify = mock_classify,
    };
    runtime_capability_helpers_bind_platform(&platform);
    test_external_audience();
    test_registry_wrappers();
    test_type6_tuple();
    test_address_policy();
    test_entry_type();
    expect_trap(invoke_invalid_audience_bit);
    expect_trap(invoke_truncated_type6_tuple);
    expect_trap(invoke_zero_rights_fatal);
    puts("runtime capability helper tests passed");
    return 0;
}

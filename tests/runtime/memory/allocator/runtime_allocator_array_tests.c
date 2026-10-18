#include "avp/runtime/memory/allocator/runtime_allocator_array.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    _Alignas(64) uint8_t storage[1024];
    RuntimeAllocatorRoute primary;
    RuntimeAllocatorRoute fallback;
    size_t cursor;
    size_t payload_size;
    size_t allocation_size;
    size_t alignment;
    uint32_t select_count;
    uint32_t allocation_count;
} MockAllocator;

static void expect_fatal(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void align_with_non_power_of_two(void) {
    (void)runtime_allocator_align_up_checked(32, 3);
}

static void multiply_overflow(void) {
    size_t allocation_size = 0;
    size_t payload_size = 0;
    runtime_allocator_compute_array_sizes_checked(
        16,
        SIZE_MAX,
        2,
        &allocation_size,
        &payload_size
    );
}

static RuntimeAllocatorRoute *mock_select_route(
    void *context,
    bool primary
) {
    MockAllocator *allocator = context;
    ++allocator->select_count;
    return primary ? &allocator->primary : &allocator->fallback;
}

static RuntimeFirebloomSpan mock_allocate(
    void *context,
    RuntimeAllocatorRoute *route,
    size_t payload_size,
    size_t allocation_size,
    size_t alignment,
    const RuntimeFirebloomType *type
) {
    MockAllocator *allocator = context;
    assert(route == &allocator->primary || route == &allocator->fallback);
    const uintptr_t base = (uintptr_t)allocator->storage;
    const uintptr_t current = runtime_allocator_align_up_checked(
        base + allocator->cursor,
        alignment
    );
    const size_t offset = current - base;
    assert(offset + allocation_size <= sizeof(allocator->storage));
    allocator->cursor = offset + allocation_size;
    allocator->payload_size = payload_size;
    allocator->allocation_size = allocation_size;
    allocator->alignment = alignment;
    ++allocator->allocation_count;
    return (RuntimeFirebloomSpan){
        .current = (void *)current,
        .lower_bound = (void *)current,
        .upper_bound = (void *)(current + allocation_size),
        .type = type,
    };
}

static void test_geometry(void) {
    const RuntimeFirebloomType type_24 = {
        .identity = 1,
        .element_size = 24,
        .bytewise_copyable = true,
    };
    const RuntimeAllocatorElementGeometry geometry =
        runtime_firebloom_type_element_geometry(&type_24);
    assert(geometry.element_size == 24);
    assert(geometry.alignment == 8);
    assert(runtime_firebloom_type_element_geometry(0).element_size == 0);
    assert(runtime_allocator_align_up_checked(33, 16) == 48);

    size_t allocation_size = 0;
    size_t payload_size = 0;
    runtime_allocator_compute_array_sizes_checked(
        16,
        24,
        3,
        &allocation_size,
        &payload_size
    );
    assert(payload_size == 72);
    assert(allocation_size == 80);
}

static void test_untyped_allocations(
    MockAllocator *allocator,
    const RuntimeFirebloomType *byte_type
) {
    allocator->primary.allocation_type = byte_type;
    RuntimeFirebloomSpan span = runtime_allocator_allocate_bytes(
        &allocator->primary,
        17
    );
    assert(span.type == byte_type);
    assert(allocator->payload_size == 17);
    assert(allocator->allocation_size == 32);
    assert(allocator->alignment == 16);

    span = runtime_allocator_allocate_array(
        &allocator->primary,
        6,
        5
    );
    assert(allocator->payload_size == 30);
    assert(allocator->allocation_size == 32);

    span = runtime_allocator_allocate_array_aligned(
        &allocator->primary,
        9,
        5,
        32
    );
    assert((uintptr_t)span.current % 32 == 0);
    assert(allocator->payload_size == 45);
    assert(allocator->allocation_size == 64);
}

static void test_typed_allocations(
    MockAllocator *allocator,
    const RuntimeFirebloomType *typed_type
) {
    RuntimeAllocatorTypedRange range = {
        .route = 0,
        .type = typed_type,
    };
    const RuntimeAllocatorTypedRange resolved =
        runtime_allocator_resolve_typed_range_root_slot(range);
    assert(resolved.route == &allocator->primary);
    assert(resolved.type == typed_type);

    RuntimeFirebloomSpan span =
        runtime_allocator_allocate_typed_instance(range);
    assert(span.type == typed_type);
    assert(allocator->payload_size == 24);
    assert(allocator->allocation_size == 24);
    assert(allocator->alignment == 8);

    span = runtime_allocator_allocate_typed_array_current(range, 3);
    assert(allocator->payload_size == 72);
    assert(allocator->allocation_size == 72);

    span = runtime_allocator_allocate_typed_array_aligned(range, 2, 64);
    assert((uintptr_t)span.current % 64 == 0);
    assert(allocator->payload_size == 48);
    assert(allocator->allocation_size == 64);

    span = runtime_allocator_outlined_allocate_one_typed_element_checked(
        range
    );
    assert(span.type == typed_type);
    assert(allocator->payload_size == 24);
}

int main(void) {
    static const RuntimeFirebloomType byte_type = {
        .identity = UINT64_C(0x62797465),
        .element_size = 1,
        .bytewise_copyable = true,
    };
    static const RuntimeFirebloomType typed_type = {
        .identity = UINT64_C(0x74797065),
        .element_size = 24,
        .bytewise_copyable = true,
    };
    MockAllocator allocator = {0};
    const RuntimeAllocatorArrayPlatform platform = {
        .context = &allocator,
        .select_route = mock_select_route,
        .allocate = mock_allocate,
    };
    runtime_allocator_array_bind_platform(&platform);

    test_geometry();
    expect_fatal(align_with_non_power_of_two);
    expect_fatal(multiply_overflow);
    test_untyped_allocations(&allocator, &byte_type);
    test_typed_allocations(&allocator, &typed_type);
    assert(allocator.select_count == 5);
    assert(allocator.allocation_count == 7);
    puts("runtime allocator array tests passed");
    return 0;
}

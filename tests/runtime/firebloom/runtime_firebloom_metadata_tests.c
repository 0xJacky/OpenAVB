#include "avp/runtime/firebloom/runtime_firebloom_metadata.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static RuntimeAllocatorRoute primary_route;
static RuntimeAllocatorRoute fallback_route;
static _Alignas(0x80) uint8_t allocation_storage[0x100];
static const RuntimeFirebloomType allocation_type = {
    .identity = 0x144f18,
    .element_size = 1,
    .bytewise_copyable = true,
};
static RuntimeAllocatorRoute *observed_route;
static bool observed_clear;
static size_t observed_size;
static size_t observed_alignment;

_Noreturn void pvpanic_trigger(void) {
    raise(SIGABRT);
    __builtin_unreachable();
}

static RuntimeFirebloomDescriptorView descriptor_view(
    RuntimeFirebloomDescriptor *descriptor
) {
    return (RuntimeFirebloomDescriptorView){
        .current = descriptor,
        .lower_bound = descriptor,
        .upper_bound = descriptor + 1,
        .context = 0x145e20,
    };
}

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

static void allocate_storage(
    void *context,
    RuntimeAllocatorRoute *route,
    bool clear,
    size_t size,
    size_t alignment,
    RuntimeFirebloomSpan *allocation
) {
    assert(context == allocation_storage);
    observed_route = route;
    observed_clear = clear;
    observed_size = size;
    observed_alignment = alignment;
    *allocation = (RuntimeFirebloomSpan){
        .current = allocation_storage,
        .lower_bound = allocation_storage,
        .upper_bound = allocation_storage + sizeof(allocation_storage),
        .type = &allocation_type,
    };
}

static void test_descriptor_accessors(void) {
    RuntimeFirebloomDescriptor descriptor = {
        .instance_size = 0x58,
        .element_shift = 3,
        .type_identity = UINT64_C(0x145da8),
        .instance_alignment = 0x20,
        .variant_shift = 7,
    };
    const RuntimeFirebloomDescriptorView view = descriptor_view(&descriptor);
    assert(runtime_firebloom_descriptor_instance_size(view) == 0x58);
    assert(runtime_firebloom_descriptor_element_shift(view) == 3);
    assert(runtime_firebloom_descriptor_type_identity(view) == 0x145da8);
    assert(runtime_firebloom_descriptor_instance_alignment(view) == 0x20);
    assert(runtime_firebloom_descriptor_variant_shift(view) == 7);

    size_t size = 0;
    size_t alignment = 0;
    runtime_firebloom_descriptor_allocation_requirements(
        view,
        &size,
        &alignment
    );
    assert(size == 0x58);
    assert(alignment == 0x40);

    descriptor.instance_size = 0x20;
    descriptor.instance_alignment = 0x80;
    runtime_firebloom_descriptor_allocation_requirements(
        view,
        &size,
        &alignment
    );
    assert(size == 0x80);
    assert(alignment == 0x80);
}

static void test_typed_storage_allocation(void) {
    RuntimeFirebloomDescriptor descriptor = {
        .instance_size = 0x90,
        .instance_alignment = 0x80,
    };
    runtime_allocator_route_initialize(&primary_route, 1, 2);
    runtime_allocator_route_initialize(&fallback_route, 3, 4);
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){
        .primary = &primary_route,
        .fallback = &fallback_route,
    });
    observed_route = 0;
    observed_clear = false;
    observed_size = 0;
    observed_alignment = 0;
    RuntimeFirebloomSpan allocation = {0};
    runtime_firebloom_allocate_typed_storage(
        descriptor_view(&descriptor),
        &(RuntimeFirebloomAllocationProfile){
            .allocate = allocate_storage,
            .context = allocation_storage,
            .allocation_type = &allocation_type,
        },
        &allocation
    );
    assert(observed_route == &fallback_route);
    assert(observed_clear);
    assert(observed_size == 0x90);
    assert(observed_alignment == 0x80);
    assert(allocation.current == allocation_storage);
}

static void fatal_truncated_descriptor(void) {
    RuntimeFirebloomDescriptor descriptor = {0};
    RuntimeFirebloomDescriptorView view = descriptor_view(&descriptor);
    view.upper_bound = (uint8_t *)&descriptor + 0x2c;
    (void)runtime_firebloom_descriptor_variant_shift(view);
}

static void fatal_missing_allocator_route(void) {
    RuntimeFirebloomDescriptor descriptor = {
        .instance_size = 0x40,
        .instance_alignment = 0x40,
    };
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){0});
    RuntimeFirebloomSpan allocation = {0};
    runtime_firebloom_allocate_typed_storage(
        descriptor_view(&descriptor),
        &(RuntimeFirebloomAllocationProfile){
            .allocate = allocate_storage,
            .context = allocation_storage,
            .allocation_type = &allocation_type,
        },
        &allocation
    );
}

int main(void) {
    test_descriptor_accessors();
    test_typed_storage_allocation();
    expect_fatal(fatal_truncated_descriptor);
    expect_fatal(fatal_missing_allocator_route);
}

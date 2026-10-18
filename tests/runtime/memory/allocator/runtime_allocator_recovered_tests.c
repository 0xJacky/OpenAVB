#include "avp/runtime/memory/allocator/runtime_allocator_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


static const RuntimeFirebloomType byte_type = {
    .identity = UINT64_C(0x62797465),
    .element_size = 1,
    .bytewise_copyable = true,
};

static unsigned enter_count;
static unsigned leave_count;
static unsigned callback_count;
static RuntimeSupportAllocationCallbackArguments callback_arguments;

void runtime_bzero(void *destination, size_t size) {
    memset(destination, 0, size);
}

_Noreturn void runtime_firebloom_panic(void) {
    __builtin_trap();
}

static void critical_enter(void) {
    ++enter_count;
}

static void critical_leave(void) {
    ++leave_count;
}

static void allocation_callback(
    void *allocation,
    size_t element_count,
    size_t element_size,
    const void *tag,
    uint64_t flags
) {
    ++callback_count;
    callback_arguments = (RuntimeSupportAllocationCallbackArguments){
        allocation,
        element_count,
        element_size,
        tag,
        flags,
    };
}

static uintptr_t relocate_storage(uintptr_t base, size_t size) {
    (void)size;
    return base + 0x100;
}

static void expect_fatal(void (*operation)(void)) {
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

static void fatal_double_enter(void) {
    runtime_allocator_critical_section_enter();
    runtime_allocator_critical_section_enter();
}

static void fatal_invalid_tuple(void) {
    uint8_t bytes[sizeof(RuntimeAllocatorRoute)] = {0};
    (void)runtime_domain_allocator_route_tuple_validate_shared(
        (RuntimeFirebloomSpan){
            .current = bytes + 1,
            .lower_bound = bytes,
            .upper_bound = bytes + 2,
            .type = &byte_type,
        }
    );
}

static void fatal_metadata_exhaustion(void) {
    alignas(16) uint8_t bytes[0x20] = {0};
    RuntimeAllocatorMetadataArena arena = {
        (uintptr_t)bytes,
        (uintptr_t)bytes + 1,
        (uintptr_t)bytes + sizeof(bytes),
    };
    (void)runtime_allocator_metadata_node_allocate(&arena);
}

int main(void) {
    alignas(64) uint8_t heap[0x8000];
    memset(heap, 0xa5, sizeof(heap));
    RuntimeAllocatorRoute *route = runtime_allocator_route_create_in_place(
        &byte_type,
        heap,
        sizeof(heap)
    );
    assert(route == (RuntimeAllocatorRoute *)(void *)heap);
    assert(route->allocation_type == &byte_type);
    assert(runtime_allocator_minimum_alignment() == 0x40);

    const RuntimeAllocatorRecoveredPlatform platform = {
        .critical_enter = critical_enter,
        .critical_leave = critical_leave,
        .allocation_callback = allocation_callback,
    };
    runtime_allocator_recovered_bind_platform(&platform);
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){route, 0});

    RuntimeFirebloomSpan selected = runtime_allocator_select_active_route();
    assert(selected.current == route);
    assert((uint8_t *)selected.upper_bound - (uint8_t *)selected.lower_bound ==
        sizeof(*route));
    runtime_allocator_require_active_route_no_output();

    RuntimeAllocatorRangeRoot *root = route->reserved_50;
    RuntimeFirebloomSpan first = runtime_allocator_allocate_array_checked(
        route,
        root,
        16,
        37,
        1
    );
    assert(((uintptr_t)first.current & 15) == 0);
    assert((uint8_t *)first.upper_bound - (uint8_t *)first.current == 37);
    assert(runtime_allocator_active_allocation_size(first.current) == 48);
    for (size_t index = 0; index < 48; ++index) {
        assert(((uint8_t *)first.current)[index] == 0);
    }
    memset(first.current, 0x5a, 48);
    runtime_allocator_release_allocation(
        first.current,
        first.lower_bound,
        first.upper_bound,
        first.type
    );
    assert(runtime_allocator_active_allocation_size(first.current) == 0);

    RuntimeFirebloomSpan reused = runtime_allocator_allocate_array_checked(
        route,
        root,
        16,
        37,
        1
    );
    assert(reused.current == first.current);
    assert(enter_count == 2 && leave_count == 2);

    void *malloc_allocation = runtime_malloc(23);
    assert(malloc_allocation != 0);
    assert(runtime_allocator_active_allocation_size(malloc_allocation) == 23);

    const char tag[] = "support";
    void *support = runtime_support_allocate_or_panic(31, tag);
    assert(support != 0);
    assert(callback_count == 1);
    assert(callback_arguments.allocation == support);
    assert(callback_arguments.element_count == 31);
    assert(callback_arguments.element_size == 1);
    assert(callback_arguments.tag == tag);
    assert(callback_arguments.flags == 0);
    assert(runtime_support_release_allocation_checked(support));
    assert(!runtime_support_release_allocation_checked(support));
    assert(runtime_support_release_allocation_checked(0));

    RuntimeAllocatorTypeDescriptor descriptor = {
        .type = &byte_type,
        .bindings = 0,
    };
    RuntimeAllocatorRangeRoot **slot =
        runtime_allocator_descriptor_resolve_route_root_slot(
            route,
            route,
            &descriptor
        );
    assert(slot != 0 && *slot != 0 && (*slot)->type == &byte_type);
    assert(slot == runtime_allocator_descriptor_resolve_route_root_slot(
        route,
        route,
        &descriptor
    ));

    RuntimeAllocatorNode extent = {
        .lower_bound = 0x1000,
        .upper_bound = 0x1080,
    };
    RuntimeAllocatorRecoveredPlatform relocating_platform = platform;
    relocating_platform.storage_identity = relocate_storage;
    runtime_allocator_recovered_bind_platform(&relocating_platform);
    runtime_allocator_extent_rebase_storage(&extent);
    assert(extent.lower_bound == 0x1100 && extent.upper_bound == 0x1180);

    RuntimeFirebloomSpan route_tuple = {
        .current = route,
        .lower_bound = route,
        .upper_bound = (uint8_t *)route + sizeof(*route),
        .type = &byte_type,
    };
    assert(runtime_domain_allocator_route_tuple_validate_thunk_a(route_tuple).current ==
        route);
    assert(runtime_domain_allocator_route_tuple_validate_thunk_b(route_tuple).current ==
        route);

    RuntimeAllocatorArrayCheckedCallContext untyped =
        runtime_allocator_outlined_prepare_untyped_array_checked_call(route, 9);
    assert(untyped.route == route && untyped.element_count == 9 &&
        untyped.element_size == 1 && untyped.result == 0);
    assert(runtime_allocator_outlined_load_result_span_from_sp(route_tuple).current ==
        route);
    RuntimeAllocatorCheckedCallContext typed =
        runtime_allocator_outlined_prepare_typed_array_checked_call(route, slot);
    assert(typed.route == route && typed.range_root_slot == slot);
    RuntimeAllocatorRegisterPair pair =
        runtime_allocator_outlined_save_x0_x1_to_x20_x19(11, 22);
    assert(pair.first == 11 && pair.second == 22);

    RuntimeAllocatorRoute *resolved_route = 0;
    RuntimeAllocatorRangeRoot **resolved_slot = 0;
    assert(runtime_support_resolve_allocator_context(
        &resolved_route,
        &resolved_slot
    ) == resolved_slot);
    assert(resolved_route == route && resolved_slot == &route->reserved_50);

    RuntimeFirebloomSpan routes_view =
        runtime_current_domain_allocator_routes_bounded_view();
    RuntimeAllocatorRoutes *routes = routes_view.current;
    assert(routes->primary == route && routes->fallback == 0);

    runtime_allocator_recovered_bind_platform(&platform);
    assert(!runtime_allocator_current_domain_uses_boot_route());
    RuntimeFirebloomSpan bump = runtime_bump_allocate_bounded_bytes(19);
    assert(bump.current != 0);
    assert((uint8_t *)bump.upper_bound - (uint8_t *)bump.current == 19);

    expect_fatal(fatal_invalid_tuple);
    expect_fatal(fatal_metadata_exhaustion);
    expect_fatal(fatal_double_enter);
    return 0;
}

#include "avp/runtime/memory/allocator/runtime_allocator.h"
#include "avp/runtime/exception/pvpanic.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static RuntimeAllocatorRoute primary_route;
static RuntimeAllocatorRoute fallback_route;
static RuntimeAllocatorNode primary_node;
static RuntimeAllocatorNode fallback_node;
static _Alignas(0x4000) uint8_t primary_storage[0x8000];
static uint8_t fallback_storage[0x80];
static uintptr_t unmapped_address;
static size_t unmapped_size;

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

static void clear_bytes(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static void fill_bytes(void *storage, size_t size, uint8_t value) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = value;
    }
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

static void record_unmap(uintptr_t address, size_t size) {
    unmapped_address = address;
    unmapped_size = size;
}

static void initialize_routes(void) {
    clear_bytes(&primary_route, sizeof(primary_route));
    clear_bytes(&fallback_route, sizeof(fallback_route));
    clear_bytes(&primary_node, sizeof(primary_node));
    clear_bytes(&fallback_node, sizeof(fallback_node));
    runtime_allocator_route_initialize(&primary_route, 1, 2);
    runtime_allocator_route_initialize(&fallback_route, 3, 4);
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){
        .primary = &primary_route,
        .fallback = &fallback_route,
    });
    runtime_allocator_publish_mapping_profile(
        (RuntimeAllocatorMappingProfile){0}
    );
}

static void test_primary_and_fallback_release(void) {
    initialize_routes();
    fill_bytes(primary_storage, 0x40, 0xa5);
    fill_bytes(fallback_storage, sizeof(fallback_storage), 0x5a);
    runtime_allocator_route_track(
        &primary_route,
        &primary_node,
        primary_storage,
        0x40,
        0
    );
    runtime_allocator_route_track(
        &fallback_route,
        &fallback_node,
        fallback_storage,
        sizeof(fallback_storage),
        0
    );

    runtime_allocator_release_address(primary_storage);
    runtime_allocator_release_address(fallback_storage);
    for (size_t index = 0; index < 0x40; ++index) {
        assert(primary_storage[index] == 0);
    }
    for (size_t index = 0; index < sizeof(fallback_storage); ++index) {
        assert(fallback_storage[index] == 0);
    }
    assert(primary_route.allocations == 0);
    assert(primary_route.recycled_nodes == &primary_node);
    assert(fallback_route.allocations == 0);
    assert(fallback_route.recycled_nodes == &fallback_node);
}

static void test_unlink_and_recycle_are_distinct_lifecycle_steps(void) {
    initialize_routes();
    fill_bytes(primary_storage, 0x40, 0xa5);
    runtime_allocator_route_track(
        &primary_route, &primary_node, primary_storage, 0x40,
        &fallback_route
    );
    RuntimeAllocatorUnlinkedAllocation unlinked;
    assert(runtime_allocator_route_unlink_allocation(
        &primary_route, primary_storage, &unlinked
    ));
    assert(primary_route.allocations == NULL);
    assert(primary_route.recycled_nodes == NULL);
    assert(unlinked.route == &primary_route);
    assert(unlinked.free_range_owner == &fallback_route);
    assert(unlinked.node == &primary_node);
    assert(unlinked.allocation == primary_storage);
    assert(primary_storage[0] == 0xa5);
    runtime_allocator_recycle_unlinked_allocation(&unlinked);
    assert(primary_route.recycled_nodes == &primary_node);
    assert(primary_storage[0] == 0);
    assert(unlinked.route == NULL);
}

static void test_static_route_address(void) {
    initialize_routes();
    primary_route.owned_lower_bound = (uintptr_t)primary_storage;
    primary_route.owned_upper_bound = (uintptr_t)primary_storage + 0x40;
    fill_bytes(primary_storage, 0x40, 0x6c);
    assert(runtime_allocator_route_release(
        &primary_route,
        primary_storage + 0x20
    ));
    assert(primary_storage[0x20] == 0x6c);
}

static void test_bounded_mapping_release(void) {
    initialize_routes();
    fill_bytes(primary_storage, sizeof(primary_storage), 0x77);
    runtime_allocator_route_track(
        &primary_route,
        &primary_node,
        primary_storage,
        sizeof(primary_storage),
        0
    );
    unmapped_address = 0;
    unmapped_size = 0;
    runtime_allocator_publish_mapping_profile(
        (RuntimeAllocatorMappingProfile){
            .lower_bound = (uintptr_t)primary_storage - 0x4000,
            .upper_bound = (uintptr_t)primary_storage +
                sizeof(primary_storage) + 0x4000,
            .forbidden_mask = 0,
            .unmap = record_unmap,
        }
    );
    runtime_allocator_release_bounded((RuntimeFirebloomSpan){
        .current = primary_storage,
        .lower_bound = primary_storage,
        .upper_bound = primary_storage + sizeof(primary_storage),
        .type = 0,
    });
    assert(unmapped_address == (uintptr_t)primary_storage);
    assert(unmapped_size == sizeof(primary_storage));
    for (size_t index = 0; index < sizeof(primary_storage); ++index) {
        assert(primary_storage[index] == 0);
    }
}

static void release_unknown(void) {
    initialize_routes();
    runtime_allocator_release_address(primary_storage);
}

static void track_duplicate(void) {
    initialize_routes();
    runtime_allocator_route_track(
        &primary_route,
        &primary_node,
        primary_storage,
        0x40,
        0
    );
    runtime_allocator_route_track(
        &primary_route,
        &fallback_node,
        primary_storage,
        0x40,
        0
    );
}

int main(void) {
    test_unlink_and_recycle_are_distinct_lifecycle_steps();
    test_primary_and_fallback_release();
    test_static_route_address();
    test_bounded_mapping_release();
    expect_fatal(release_unknown);
    expect_fatal(track_duplicate);
}

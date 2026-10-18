#include "avp/runtime/memory/allocator/runtime_allocator_route_tuple.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


static bool tuple_equal(
    RuntimeAllocatorRouteTuple left,
    RuntimeAllocatorRouteTuple right
) {
    return left.current == right.current &&
        left.lower_bound == right.lower_bound &&
        left.upper_bound == right.upper_bound &&
        left.cookie == right.cookie;
}

static RuntimeAllocatorRouteTuple bounded_route(
    RuntimeAllocatorRoute *route
) {
    return (RuntimeAllocatorRouteTuple){
        .current = route,
        .lower_bound = (uintptr_t)route,
        .upper_bound = (uintptr_t)route + sizeof(*route),
        .cookie = UINT64_C(0x724f555445),
    };
}

static void test_validation_clones(RuntimeAllocatorRouteTuple tuple) {
    assert(runtime_allocator_validate_route_tuple(tuple) == tuple.current);
    assert(runtime_allocator_validate_route_tuple_tail_clone_1(tuple) ==
        tuple.current);
    assert(runtime_allocator_validate_route_tuple_tail_clone_2(tuple) ==
        tuple.current);
    assert(runtime_allocator_validate_route_tuple_tail_clone_3(tuple) ==
        tuple.current);
    assert(runtime_allocator_validate_route_tuple_tail_clone_4(tuple) ==
        tuple.current);

    RuntimeAllocatorRouteTuple invalid = tuple;
    invalid.lower_bound = (uintptr_t)tuple.current + 1;
    assert(runtime_allocator_validate_route_tuple(invalid) == 0);
    invalid = tuple;
    invalid.upper_bound = (uintptr_t)tuple.current +
        sizeof(*tuple.current) - 1;
    assert(runtime_allocator_validate_route_tuple(invalid) == 0);
    assert(runtime_allocator_validate_route_tuple(
        (RuntimeAllocatorRouteTuple){0}
    ) == 0);
}

static void test_tuple_moves(RuntimeAllocatorRouteTuple tuple) {
    const RuntimeAllocatorRouteTuple empty =
        runtime_allocator_outlined_prepare_route_result_tuple();
    assert(tuple_equal(empty, (RuntimeAllocatorRouteTuple){0}));
    assert(tuple_equal(
        runtime_allocator_outlined_save_route_tuple_x19_x22(tuple),
        tuple
    ));
    assert(tuple_equal(
        runtime_allocator_outlined_restore_route_tuple_x19_x22(tuple),
        tuple
    ));
    assert(runtime_allocator_outlined_compare_route_current_to_upper(tuple));
    tuple.upper_bound = (uintptr_t)tuple.current;
    assert(!runtime_allocator_outlined_compare_route_current_to_upper(tuple));
}

static void test_record_and_boot_route(RuntimeAllocatorRouteTuple tuple) {
    const RuntimeAllocatorRouteRecord record = {
        .metadata = {1, 2, 3},
        .route = tuple,
    };
    assert(tuple_equal(
        runtime_allocator_outlined_load_record_route_to_frame(&record),
        tuple
    ));
    assert(tuple_equal(
        runtime_allocator_outlined_load_record_route(&record),
        tuple
    ));
    runtime_allocator_publish_boot_route_tuple(tuple);
    assert(tuple_equal(runtime_allocator_boot_route_tuple(), tuple));
}

int main(void) {
    RuntimeAllocatorRoute route = {0};
    const RuntimeAllocatorRouteTuple tuple = bounded_route(&route);
    test_validation_clones(tuple);
    test_tuple_moves(tuple);
    test_record_and_boot_route(tuple);
    assert(runtime_allocator_route_source_token() ==
        UINT64_C(0x040ce68155485954));
    puts("runtime allocator route tuple tests passed");
    return 0;
}

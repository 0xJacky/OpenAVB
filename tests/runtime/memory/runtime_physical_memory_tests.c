#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/memory/runtime_physical_memory.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static const RuntimeFirebloomType byte_type = {
    .identity = 0x42,
    .element_size = 1,
    .bytewise_copyable = true,
};

static uint8_t physical_storage[0x40];
static uint8_t active_storage[0x40];
static uint8_t metadata;
static RuntimePhysicalMemoryRegion records[4];

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

static void clear_bytes(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
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

static void initialize_registry(void) {
    clear_bytes(records, sizeof(records));
    records[0] = (RuntimePhysicalMemoryRegion){
        .physical_base = (uintptr_t)physical_storage,
        .active_alias = (uintptr_t)active_storage,
        .size = sizeof(active_storage),
        .descriptor = (UINT64_C(0x80000009) << 16) | 0x00a6,
        .metadata = {
            .current = &metadata,
            .lower_bound = &metadata,
            .upper_bound = &metadata + 1,
            .type = &byte_type,
        },
    };
    records[1] = (RuntimePhysicalMemoryRegion){
        .physical_base = 0,
        .active_alias = 0,
        .size = 0,
        .descriptor = (UINT64_C(0x80010009) << 16) | 0x0102,
    };
    records[2] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINTPTR_MAX,
        .active_alias = 0,
        .size = SIZE_MAX,
        .descriptor = (UINT64_C(0x80010009) << 16) | 0x0103,
    };
    records[3] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINTPTR_MAX - 0x10,
        .active_alias = 0,
        .size = 0x20,
        .descriptor = (UINT64_C(0x80010009) << 16) | 0x0104,
    };
    runtime_physical_memory_registry_initialize(
        (RuntimePhysicalMemoryRegistry){
            .records = records,
            .count = 4,
        }
    );
}

static void test_lookup_and_bounds(void) {
    initialize_registry();
    RuntimeFirebloomSpan region = runtime_physical_memory_region_lookup(0xa6);
    assert(region.current == &records[0]);
    assert((uint8_t *)region.upper_bound - (uint8_t *)region.lower_bound ==
        0x20);
    assert(runtime_physical_memory_region_id(region.current) == 0xa6);
    assert(runtime_physical_memory_region_attributes(region.current) ==
        UINT64_C(0x80000009));
    assert(runtime_physical_memory_region_is_defined(region));
    assert(runtime_physical_memory_region_is_live(region));
    assert(runtime_physical_memory_region_active_base(region) ==
        (uintptr_t)active_storage);
    assert(runtime_physical_memory_region_physical_upper_bound(region) ==
        (uintptr_t)physical_storage + sizeof(physical_storage));
    assert(runtime_physical_memory_region_active_upper_bound(region) ==
        (uintptr_t)active_storage + sizeof(active_storage));

    RuntimeFirebloomSpan empty = runtime_physical_memory_region_lookup(0x102);
    assert(runtime_physical_memory_region_is_defined(empty));
    assert(!runtime_physical_memory_region_is_live(empty));
    assert(runtime_physical_memory_region_active_upper_bound(empty) == 0);

    RuntimeFirebloomSpan undefined =
        runtime_physical_memory_region_lookup(0x103);
    assert(!runtime_physical_memory_region_is_defined(undefined));
    assert(!runtime_physical_memory_region_is_live(undefined));
    assert(runtime_physical_memory_region_lookup(0x200).current == 0);
    assert(runtime_physical_memory_region_lookup(0x300).current == 0);
}

static void test_zero_active_alias(void) {
    initialize_registry();
    for (size_t index = 0; index < sizeof(active_storage); ++index) {
        physical_storage[index] = 0x11;
        active_storage[index] = 0xa5;
    }
    runtime_physical_memory_region_zero(
        runtime_physical_memory_region_lookup(0xa6)
    );
    for (size_t index = 0; index < sizeof(active_storage); ++index) {
        assert(active_storage[index] == 0);
        assert(physical_storage[index] == 0x11);
    }
}

static void test_matching_registry(void) {
    runtime_physical_memory_registry_initialize_matching();
    const RuntimePhysicalMemoryRegistry *registry =
        runtime_physical_memory_registry();
    assert(registry->count == RUNTIME_PHYSICAL_REGION_COUNT_MATCHING);
    RuntimeFirebloomSpan region = runtime_physical_memory_region_lookup(0x2d);
    assert(runtime_physical_memory_region_active_base(region) == 0x70074000);
    assert(runtime_physical_memory_region_physical_upper_bound(region) ==
        0x90074000);
}

static void lookup_zero(void) {
    initialize_registry();
    (void)runtime_physical_memory_region_lookup(0);
}

static void physical_upper_for_empty(void) {
    initialize_registry();
    (void)runtime_physical_memory_region_physical_upper_bound(
        runtime_physical_memory_region_lookup(0x102)
    );
}

static void physical_upper_overflow(void) {
    initialize_registry();
    (void)runtime_physical_memory_region_physical_upper_bound(
        runtime_physical_memory_region_lookup(0x104)
    );
}

int main(void) {
    test_lookup_and_bounds();
    test_zero_active_alias();
    test_matching_registry();
    expect_fatal(lookup_zero);
    expect_fatal(physical_upper_for_empty);
    expect_fatal(physical_upper_overflow);
}

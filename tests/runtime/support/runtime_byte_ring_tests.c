#include "avp/runtime/support/runtime_byte_ring.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


static uint32_t push_count;

static void record_push(void *context) {
    assert(context == &push_count);
    ++push_count;
}

static RuntimeFirebloomSpan byte_span(
    uint8_t *storage,
    size_t size,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = storage,
        .lower_bound = storage,
        .upper_bound = storage + size,
        .type = type,
    };
}

static RuntimeByteRingControlTuple control_tuple(RuntimeByteRing *ring) {
    return (RuntimeByteRingControlTuple){
        .current = ring,
        .lower_bound = (uintptr_t)ring,
        .upper_bound = (uintptr_t)ring + sizeof(*ring),
        .cookie = UINT64_C(0x52494e47),
    };
}

static void test_initialize_and_write(void) {
    static const RuntimeFirebloomType byte_type = {
        .identity = UINT64_C(0x62797465),
        .element_size = 1,
        .bytewise_copyable = true,
    };
    RuntimeByteRing ring = {0};
    uint8_t destination[8];
    uint8_t source[12];
    for (uint8_t index = 0; index < sizeof(source); ++index) {
        source[index] = (uint8_t)(index + 1);
    }
    const RuntimeByteRingControlTuple control = control_tuple(&ring);
    const RuntimeFirebloomSpan storage = byte_span(
        destination,
        sizeof(destination),
        &byte_type
    );
    const RuntimeByteRingPhysicalRegion physical = {
        .current = destination,
        .lower_bound = (uintptr_t)destination,
        .upper_bound = (uintptr_t)destination + sizeof(destination),
        .cookie = UINT64_C(0x50485953),
    };
    push_count = 0;
    const RuntimeByteRingControlTuple initialized =
        runtime_byte_ring_initialize(
            control,
            storage,
            physical,
            sizeof(destination),
            record_push,
            0,
            &push_count,
            UINT64_C(7)
        );
    assert(initialized.current == &ring);
    assert(ring.capacity == 8);
    assert(ring.mask == 7);
    assert(ring.write_index == 0);
    assert(ring.produced_count == 0);
    assert(ring.options == 7);
    assert(runtime_byte_ring_validate_control_tuple(control) == &ring);
    assert(runtime_byte_ring_validate_byte_storage(storage, 8) ==
        destination);

    const RuntimeFirebloomSpan source_span = byte_span(
        source,
        sizeof(source),
        &byte_type
    );
    assert(runtime_byte_ring_write(control, source_span, 5) == 5);
    assert(runtime_byte_ring_write(control, source_span, 5) == 3);
    assert(runtime_byte_ring_write(control, source_span, 1) == 0);
    assert(ring.produced_count == 8);
    assert(ring.write_index == 0);
    assert(push_count == 8);
    for (uint8_t index = 0; index < sizeof(destination); ++index) {
        const uint8_t expected = index < 5 ? (uint8_t)(index + 1) :
            (uint8_t)(index - 4);
        assert(destination[index] == expected);
    }

    const RuntimeByteRingPhysicalRegion restored =
        runtime_byte_ring_restore_physical_region_tuple(physical);
    assert(restored.current == physical.current);
    assert(restored.lower_bound == physical.lower_bound);
    assert(restored.upper_bound == physical.upper_bound);
    assert(restored.cookie == physical.cookie);
    assert(runtime_byte_ring_source_token() ==
        UINT64_C(0x0926bbecb95e6db0));
}

int main(void) {
    test_initialize_and_write();
    puts("runtime byte ring tests passed");
    return 0;
}

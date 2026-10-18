#include "avp/runtime/support/runtime_byte_ring.h"


/*
 * Related public ring implementations:
 * FreeRTOS-Kernel ce221a8 StreamBuffer (MIT):
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * Zephyr 0d65ce4 ring buffer (Apache-2.0):
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * DPDK c1a46b9 rte_ring (BSD-3-Clause):
 * https://github.com/DPDK/dpdk/commit/c1a46b9d9243e922428e8a5f87fa3c6ac177dc5a
 * AVP adds bounded capability tuples and per-byte notifications to the common
 * power-of-two mask algorithm, so its 0x80 control ABI remains local.
 */

static _Noreturn void runtime_byte_ring_fail(void) {
    __builtin_trap();
}

static bool runtime_byte_ring_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

uint8_t *runtime_byte_ring_validate_byte_storage(
    RuntimeFirebloomSpan storage,
    size_t minimum_size
) {
    if (storage.current == 0 || storage.lower_bound == 0 ||
        storage.upper_bound == 0 || storage.type == 0 ||
        storage.type->element_size != 1) {
        return 0;
    }
    const uintptr_t current = (uintptr_t)storage.current;
    const uintptr_t lower = (uintptr_t)storage.lower_bound;
    const uintptr_t upper = (uintptr_t)storage.upper_bound;
    if (lower > current || current >= upper ||
        minimum_size > upper - current) {
        return 0;
    }
    return storage.current;
}

RuntimeByteRing *runtime_byte_ring_validate_control_tuple(
    RuntimeByteRingControlTuple control
) {
    if (control.current == 0) {
        return 0;
    }
    const uintptr_t current = (uintptr_t)control.current;
    if (control.lower_bound > current || current >= control.upper_bound ||
        sizeof(*control.current) > control.upper_bound - current) {
        return 0;
    }
    return control.current;
}

static bool runtime_byte_ring_physical_region_valid(
    RuntimeByteRingPhysicalRegion region
) {
    if (region.current == 0) {
        return region.lower_bound == 0 && region.upper_bound == 0 &&
            region.cookie == 0;
    }
    const uintptr_t current = (uintptr_t)region.current;
    return region.lower_bound <= current && current < region.upper_bound;
}

RuntimeByteRingControlTuple runtime_byte_ring_initialize(
    RuntimeByteRingControlTuple control,
    RuntimeFirebloomSpan storage,
    RuntimeByteRingPhysicalRegion physical_region,
    size_t capacity,
    RuntimeByteRingNotification push_notification,
    RuntimeByteRingNotification pop_notification,
    void *notification_context,
    uint64_t options
) {
    RuntimeByteRing *ring = runtime_byte_ring_validate_control_tuple(control);
    uint8_t *bytes = runtime_byte_ring_validate_byte_storage(
        storage,
        capacity
    );
    if (ring == 0 || bytes == 0 ||
        !runtime_byte_ring_is_power_of_two(capacity) ||
        capacity > UINT32_MAX ||
        !runtime_byte_ring_physical_region_valid(physical_region)) {
        runtime_byte_ring_fail();
    }

    for (size_t index = 0; index < capacity; ++index) {
        bytes[index] = 0;
    }
    ring->storage = storage;
    ring->physical_region = physical_region;
    ring->write_index = 0;
    ring->read_index = 0;
    ring->produced_count = 0;
    ring->capacity = capacity;
    ring->mask = (uint32_t)(capacity - 1);
    ring->reserved_5c = 0;
    ring->push_notification = push_notification;
    ring->pop_notification = pop_notification;
    ring->notification_context = notification_context;
    ring->options = options;
    return control;
}

size_t runtime_byte_ring_write(
    RuntimeByteRingControlTuple control,
    RuntimeFirebloomSpan source,
    size_t byte_count
) {
    RuntimeByteRing *ring = runtime_byte_ring_validate_control_tuple(control);
    if (ring == 0 ||
        runtime_byte_ring_validate_byte_storage(
            ring->storage,
            ring->capacity
        ) == 0) {
        runtime_byte_ring_fail();
    }
    const uint8_t *source_bytes =
        runtime_byte_ring_validate_byte_storage(source, byte_count);
    if (byte_count != 0 && source_bytes == 0) {
        runtime_byte_ring_fail();
    }
    if (!runtime_byte_ring_is_power_of_two(ring->capacity) ||
        ring->mask != ring->capacity - 1 ||
        ring->produced_count > ring->capacity) {
        runtime_byte_ring_fail();
    }

    uint8_t *destination = ring->storage.current;
    size_t written = 0;
    while (written < byte_count &&
           ring->produced_count != ring->capacity) {
        destination[ring->write_index] = source_bytes[written];
        ring->write_index = (ring->write_index + 1) & ring->mask;
        ++ring->produced_count;
        ++written;
        if (ring->push_notification != 0) {
            ring->push_notification(ring->notification_context);
        }
    }
    return written;
}

uint64_t runtime_byte_ring_source_token(void) {
    return UINT64_C(0x0926bbecb95e6db0);
}

RuntimeByteRingPhysicalRegion runtime_byte_ring_restore_physical_region_tuple(
    RuntimeByteRingPhysicalRegion physical_region
) {
    return physical_region;
}

#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stddef.h>
#include <stdint.h>

#define RUNTIME_BYTE_RING_PUBLIC __attribute__((used, retain))

typedef struct {
    void *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeByteRingPhysicalRegion;

_Static_assert(
    sizeof(RuntimeByteRingPhysicalRegion) == 0x20,
    "byte ring physical tuple size"
);

typedef void (*RuntimeByteRingNotification)(void *context);

typedef struct {
    RuntimeFirebloomSpan storage;
    RuntimeByteRingPhysicalRegion physical_region;
    uint32_t write_index;
    uint32_t read_index;
    uint64_t produced_count;
    uint64_t capacity;
    uint32_t mask;
    uint32_t reserved_5c;
    RuntimeByteRingNotification push_notification;
    RuntimeByteRingNotification pop_notification;
    void *notification_context;
    uint64_t options;
} RuntimeByteRing;

_Static_assert(sizeof(RuntimeByteRing) == 0x80, "byte ring control size");
_Static_assert(
    offsetof(RuntimeByteRing, write_index) == 0x40,
    "byte ring write index offset"
);
_Static_assert(
    offsetof(RuntimeByteRing, produced_count) == 0x48,
    "byte ring produced offset"
);
_Static_assert(
    offsetof(RuntimeByteRing, mask) == 0x58,
    "byte ring mask offset"
);
_Static_assert(
    offsetof(RuntimeByteRing, push_notification) == 0x60,
    "byte ring push callback offset"
);

typedef struct {
    RuntimeByteRing *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeByteRingControlTuple;

RUNTIME_BYTE_RING_PUBLIC RuntimeByteRingControlTuple runtime_byte_ring_initialize(
    RuntimeByteRingControlTuple control,
    RuntimeFirebloomSpan storage,
    RuntimeByteRingPhysicalRegion physical_region,
    size_t capacity,
    RuntimeByteRingNotification push_notification,
    RuntimeByteRingNotification pop_notification,
    void *notification_context,
    uint64_t options
);
RUNTIME_BYTE_RING_PUBLIC size_t runtime_byte_ring_write(
    RuntimeByteRingControlTuple control,
    RuntimeFirebloomSpan source,
    size_t byte_count
);
RUNTIME_BYTE_RING_PUBLIC uint8_t *runtime_byte_ring_validate_byte_storage(
    RuntimeFirebloomSpan storage,
    size_t minimum_size
);
RUNTIME_BYTE_RING_PUBLIC RuntimeByteRing *runtime_byte_ring_validate_control_tuple(
    RuntimeByteRingControlTuple control
);
RUNTIME_BYTE_RING_PUBLIC uint64_t runtime_byte_ring_source_token(void);
RUNTIME_BYTE_RING_PUBLIC RuntimeByteRingPhysicalRegion
runtime_byte_ring_restore_physical_region_tuple(
    RuntimeByteRingPhysicalRegion physical_region
);

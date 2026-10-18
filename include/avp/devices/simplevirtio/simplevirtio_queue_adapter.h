#pragma once

#include "avp/devices/simplevirtio/simple_virtio.h"

#include <stdbool.h>
#include <stdint.h>


enum {
    SIMPLEVIRTIO_ADAPTER_MAX_QUEUES = 2,
    SIMPLEVIRTIO_ADAPTER_MAX_SEGMENTS = 5,
    SIMPLEVIRTIO_ADAPTER_MMIO_LIMIT = 0x414,
};

#define SIMPLEVIRTIO_SOURCE_TOKEN UINT64_C(0x0133c360a905c0b0)

typedef enum {
    SIMPLEVIRTIO_ADAPTER_OK = 0,
    SIMPLEVIRTIO_ADAPTER_ERROR_ARGUMENT = -1,
    SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS = -2,
    SIMPLEVIRTIO_ADAPTER_ERROR_VERSION = -3,
    SIMPLEVIRTIO_ADAPTER_ERROR_DEVICE = -4,
    SIMPLEVIRTIO_ADAPTER_ERROR_QUEUE = -5,
    SIMPLEVIRTIO_ADAPTER_ERROR_DESCRIPTOR = -6,
    SIMPLEVIRTIO_ADAPTER_ERROR_BUSY = -7,
} SimpleVirtioAdapterStatus;

typedef uint32_t (*SimpleVirtioAdapterRead32)(
    void *context,
    uintptr_t base,
    uint32_t offset
);
typedef void (*SimpleVirtioAdapterWrite32)(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint32_t value
);
typedef void (*SimpleVirtioAdapterWrite64)(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint64_t value
);
typedef uint64_t (*SimpleVirtioAdapterTranslate)(
    void *context,
    uint64_t address
);

typedef struct {
    void *context;
    SimpleVirtioAdapterRead32 read32;
    SimpleVirtioAdapterWrite32 write32;
    SimpleVirtioAdapterWrite64 write64;
    SimpleVirtioAdapterTranslate translate;
} SimpleVirtioAdapterIO;

typedef struct {
    SimpleVirtioSegment descriptors[SIMPLEVIRTIO_ADAPTER_MAX_SEGMENTS];
    uint64_t descriptor_table_address;
    uint8_t descriptor_count;
    uint8_t active;
} SimpleVirtioQueueAllocation;

typedef struct {
    SimpleVirtioAdapterIO io;
    uintptr_t base;
    SimpleVirtioQueueAllocation queues[SIMPLEVIRTIO_ADAPTER_MAX_QUEUES];
    uint8_t queue_count;
    uint8_t selected_queue;
    uint8_t status;
    SimpleVirtioAdapterStatus last_error;
} SimpleVirtioQueueDevice;

typedef struct {
    SimpleVirtioQueueDevice *device;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} SimpleVirtioDeviceSpan;

typedef struct {
    uint64_t source_token;
    uint32_t line;
} SimpleVirtioDiagnostic;

#define SIMPLEVIRTIO_ADAPTER_PUBLIC __attribute__((used, retain))

SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_device_initialize(
    SimpleVirtioQueueDevice *device,
    const SimpleVirtioAdapterIO *io,
    uintptr_t base,
    uint32_t expected_device_id,
    uint8_t queue_count,
    bool driver_ready,
    const uint64_t descriptor_table_addresses[
        SIMPLEVIRTIO_ADAPTER_MAX_QUEUES
    ]
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_device_deinitialize(
    SimpleVirtioDeviceSpan span,
    uint32_t poll_limit
);
SIMPLEVIRTIO_ADAPTER_PUBLIC bool simplevirtio_mmio_read32(
    SimpleVirtioDeviceSpan span,
    uint32_t offset,
    uint32_t *value
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_mmio_read32_bounds_failure(
    SimpleVirtioQueueDevice *device
);
SIMPLEVIRTIO_ADAPTER_PUBLIC bool simplevirtio_mmio_write32(
    SimpleVirtioDeviceSpan span,
    uint32_t offset,
    uint32_t value
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_mmio_write32_bounds_failure(
    SimpleVirtioQueueDevice *device
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus simplevirtio_queue_select(
    SimpleVirtioDeviceSpan span,
    uint8_t queue_index
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_queue_available(
    SimpleVirtioDeviceSpan span,
    uint8_t queue_index,
    uint32_t *available
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_append_descriptor_segment(
    SimpleVirtioDeviceSpan span,
    uint64_t address,
    uint32_t length,
    bool device_writes,
    bool continues
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioAdapterStatus
simplevirtio_commit_descriptor(
    SimpleVirtioDeviceSpan span,
    bool wait_for_completion,
    uint32_t poll_limit
);
SIMPLEVIRTIO_ADAPTER_PUBLIC bool simplevirtio_queue_allocation_validate(
    const SimpleVirtioQueueDevice *device,
    const SimpleVirtioQueueAllocation *allocation
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioDiagnostic
simplevirtio_outlined_log_source_line(uint32_t line);
SIMPLEVIRTIO_ADAPTER_PUBLIC uint64_t simplevirtio_source_token(void);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioDeviceSpan
simplevirtio_outlined_device_span_x21(
    SimpleVirtioDeviceSpan span
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioDeviceSpan
simplevirtio_outlined_device_span_x22(
    SimpleVirtioDeviceSpan span
);
SIMPLEVIRTIO_ADAPTER_PUBLIC SimpleVirtioDeviceSpan
simplevirtio_outlined_save_device_bounds_x19_x20(
    SimpleVirtioQueueDevice *device,
    uintptr_t lower_bound,
    uintptr_t upper_bound
);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#if __has_attribute(retain)
#define SIMPLE_VIRTIO_DCD_PUBLIC __attribute__((used, retain))
#else
#define SIMPLE_VIRTIO_DCD_PUBLIC __attribute__((used))
#endif

typedef struct {
    const uint8_t *out_payload;
    uint32_t out_payload_size;
    uint8_t *in_payload;
    uint32_t in_payload_capacity;
    uint32_t in_payload_size;
    uint32_t out_payload_consumed;
    bool stalled;
} SimpleVirtioDCDTransfer;

typedef uint32_t (*SimpleVirtioDCDTimeMillis)(void *context);

SIMPLE_VIRTIO_DCD_PUBLIC void simple_virtio_dcd_bind_time_source(
    SimpleVirtioDCDTimeMillis read_millis,
    void *context
);
SIMPLE_VIRTIO_DCD_PUBLIC uint32_t tusb_time_millis_api(void);
void simple_virtio_dcd_begin(SimpleVirtioDCDTransfer *transfer);
void simple_virtio_dcd_end(void);

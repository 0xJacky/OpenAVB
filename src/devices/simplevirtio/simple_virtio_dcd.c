#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "avp/devices/simplevirtio/simple_virtio_dcd.h"

#include "device/dcd.h"

#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 1
#include <time.h>
#endif

static SimpleVirtioDCDTransfer *active_transfer;
static SimpleVirtioDCDTimeMillis bound_time_source;
static void *bound_time_context;

void simple_virtio_dcd_bind_time_source(
    SimpleVirtioDCDTimeMillis read_millis,
    void *context
) {
    bound_time_source = read_millis;
    bound_time_context = context;
}

uint32_t tusb_time_millis_api(void) {
    if (bound_time_source != 0) {
        return bound_time_source(bound_time_context);
    }
#if defined(__aarch64__) && \
    (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
    uint64_t counter = 0;
    uint64_t frequency = 0;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(counter));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    if (frequency == 0) {
        __builtin_trap();
    }
    const uint64_t millis = (counter / frequency) * UINT64_C(1000) +
        ((counter % frequency) * UINT64_C(1000)) / frequency;
    return (uint32_t)millis;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        __builtin_trap();
    }
    return (uint32_t)((uint64_t)now.tv_sec * UINT64_C(1000) +
        (uint64_t)now.tv_nsec / UINT64_C(1000000));
#endif
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, uint32_t size) {
    for (uint32_t index = 0; index < size; ++index) {
        destination[index] = source[index];
    }
}

void simple_virtio_dcd_begin(SimpleVirtioDCDTransfer *transfer) {
    transfer->in_payload_size = 0;
    transfer->out_payload_consumed = 0;
    transfer->stalled = false;
    active_transfer = transfer;
}

void simple_virtio_dcd_end(void) {
    active_transfer = 0;
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
    (void)rhport;
    (void)rh_init;
    return true;
}

bool dcd_deinit(uint8_t rhport) {
    (void)rhport;
    active_transfer = 0;
    return true;
}

bool dcd_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param) {
    (void)rhport;
    (void)cfg_id;
    (void)cfg_param;
    return true;
}

void dcd_int_handler(uint8_t rhport) {
    (void)rhport;
}

void dcd_int_enable(uint8_t rhport) {
    (void)rhport;
}

void dcd_int_disable(uint8_t rhport) {
    (void)rhport;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
    (void)dev_addr;
    dcd_event_xfer_complete(
        rhport,
        0x80,
        0,
        XFER_RESULT_SUCCESS,
        false
    );
}

void dcd_remote_wakeup(uint8_t rhport) {
    (void)rhport;
}

void dcd_connect(uint8_t rhport) {
    (void)rhport;
}

void dcd_disconnect(uint8_t rhport) {
    (void)rhport;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
    (void)rhport;
    (void)en;
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
    (void)rhport;
    (void)desc_ep;
    return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
    (void)rhport;
}

bool dcd_edpt_xfer(
    uint8_t rhport,
    uint8_t ep_addr,
    uint8_t *buffer,
    uint16_t total_bytes,
    bool is_isr
) {
    (void)is_isr;
    if (active_transfer == 0) {
        return false;
    }
    const bool device_to_host = (ep_addr & 0x80) != 0;
    if (total_bytes != 0 && device_to_host) {
        if (active_transfer->in_payload_size + total_bytes >
            active_transfer->in_payload_capacity) {
            active_transfer->stalled = true;
            return false;
        }
        copy_bytes(
            active_transfer->in_payload + active_transfer->in_payload_size,
            buffer,
            total_bytes
        );
        active_transfer->in_payload_size += total_bytes;
    } else if (total_bytes != 0) {
        if (active_transfer->out_payload_consumed + total_bytes >
            active_transfer->out_payload_size) {
            active_transfer->stalled = true;
            return false;
        }
        copy_bytes(
            buffer,
            active_transfer->out_payload +
                active_transfer->out_payload_consumed,
            total_bytes
        );
        active_transfer->out_payload_consumed += total_bytes;
    }
    dcd_event_xfer_complete(
        rhport,
        ep_addr,
        total_bytes,
        XFER_RESULT_SUCCESS,
        false
    );
    return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
    if (active_transfer != 0) {
        active_transfer->stalled = true;
    }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
}

bool dcd_edpt_iso_alloc(
    uint8_t rhport,
    uint8_t ep_addr,
    uint16_t largest_packet_size
) {
    (void)rhport;
    (void)ep_addr;
    (void)largest_packet_size;
    return false;
}

bool dcd_edpt_iso_activate(
    uint8_t rhport,
    const tusb_desc_endpoint_t *desc_ep
) {
    (void)rhport;
    (void)desc_ep;
    return false;
}

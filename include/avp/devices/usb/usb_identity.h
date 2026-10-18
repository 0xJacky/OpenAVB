#pragma once

#include "avp/platform/platform.h"

#include <stdbool.h>
#include <stdint.h>


uint16_t usb_identity_product_id(
    const AVPPlatformIdentity *identity,
    bool recovery
);

uint32_t usb_identity_emit_string(
    uint8_t index,
    uint8_t *output,
    uint32_t capacity,
    uint16_t requested_length,
    const AVPPlatformIdentity *identity,
    bool recovery,
    bool source_image_executed
);

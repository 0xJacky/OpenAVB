#include "avp/devices/usb/usb_identity.h"

#include "avp/devices/usb/usb_dfu_identity_adapter.h"


enum {
    USB_DESCRIPTOR_STRING = 3,
    RECOVERY_PRODUCT_ID = 0x1281,
};

static uint32_t minimum(uint32_t left, uint32_t right) {
    return left < right ? left : right;
}

static uint32_t text_length(const char *text, uint32_t capacity) {
    uint32_t length = 0;
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static void append_text(
    char *output,
    uint32_t *cursor,
    const char *text,
    uint32_t length
) {
    for (uint32_t index = 0; index < length; ++index) {
        output[(*cursor)++] = text[index];
    }
}

static void append_hex(
    char *output,
    uint32_t *cursor,
    uint64_t value,
    uint32_t digits
) {
    static const char alphabet[] = "0123456789ABCDEF";
    for (uint32_t index = 0; index < digits; ++index) {
        const uint32_t shift = (digits - index - 1) * 4;
        output[(*cursor)++] = alphabet[(value >> shift) & 0x0f];
    }
}

static void append_hex_bytes(
    char *output,
    uint32_t *cursor,
    const uint8_t *bytes,
    uint32_t size
) {
    for (uint32_t index = 0; index < size; ++index) {
        append_hex(output, cursor, bytes[index], 2);
    }
}

static uint32_t emit_ascii_string(
    uint8_t *output,
    uint32_t capacity,
    const char *text,
    uint32_t length,
    uint16_t requested_length
) {
    const uint32_t descriptor_size = 2 + length * 2;
    const uint32_t produced = minimum(
        minimum(descriptor_size, requested_length),
        capacity
    );
    if (produced == 0) {
        return 0;
    }
    output[0] = (uint8_t)descriptor_size;
    if (produced == 1) {
        return produced;
    }
    output[1] = USB_DESCRIPTOR_STRING;
    for (uint32_t index = 0; index < length && 2 + index * 2 < produced; ++index) {
        output[2 + index * 2] = (uint8_t)text[index];
        if (3 + index * 2 < produced) {
            output[3 + index * 2] = 0;
        }
    }
    return produced;
}

static uint32_t emit_serial(
    uint8_t *output,
    uint32_t capacity,
    uint16_t requested_length,
    const AVPPlatformIdentity *identity,
    bool source_image_executed
) {
    static const char root_tag[] = "mBoot-20457.0.13.0.11";
    static const char execution[] = " IBSS:EXECUTED";
    char serial[192];
    uint32_t cursor = 0;
    append_text(serial, &cursor, "SDOM:", 5);
    append_hex(serial, &cursor, identity->security_domain, 2);
    append_text(serial, &cursor, " CPID:", 6);
    append_hex(serial, &cursor, identity->chip_id, 4);
    append_text(serial, &cursor, " CPRV:", 6);
    append_hex(serial, &cursor, identity->chip_revision, 2);
    append_text(serial, &cursor, " CPFM:", 6);
    append_hex(serial, &cursor, identity->chip_fuse_mode, 2);
    append_text(serial, &cursor, " SCEP:", 6);
    append_hex(serial, &cursor, identity->security_epoch, 2);
    append_text(serial, &cursor, " BDID:", 6);
    append_hex(serial, &cursor, identity->board_id, 2);
    append_text(serial, &cursor, " ECID:", 6);
    append_hex(serial, &cursor, identity->ecid, 16);
    append_text(serial, &cursor, " IBFL:", 6);
    append_hex(serial, &cursor, identity->boot_flags, 2);
    append_text(serial, &cursor, " SRTG:[", 7);
    append_text(serial, &cursor, root_tag, sizeof(root_tag) - 1);
    append_text(serial, &cursor, "]", 1);
    if (source_image_executed) {
        append_text(serial, &cursor, execution, sizeof(execution) - 1);
    }
    return emit_ascii_string(
        output,
        capacity,
        serial,
        cursor,
        requested_length
    );
}

static uint32_t emit_nonce_string(
    uint8_t *output,
    uint32_t capacity,
    uint16_t requested_length,
    const AVPPlatformIdentity *identity
) {
    char text[126];
    uint32_t cursor = 0;
    append_text(text, &cursor, " NONC:", 6);
    append_hex_bytes(
        text,
        &cursor,
        identity->ap_nonce,
        sizeof(identity->ap_nonce)
    );
    append_text(text, &cursor, " SNON:", 6);
    append_hex_bytes(
        text,
        &cursor,
        identity->sep_nonce,
        sizeof(identity->sep_nonce)
    );
    append_text(text, &cursor, " DCC:0:0:0", 10);
    return emit_ascii_string(output, capacity, text, cursor, requested_length);
}

uint16_t usb_identity_product_id(
    const AVPPlatformIdentity *identity,
    bool recovery
) {
    return recovery
        ? RECOVERY_PRODUCT_ID
        : (uint16_t)(USB_DFU_PRODUCT_ID_BASE + (identity->security_domain & 3));
}

uint32_t usb_identity_emit_string(
    uint8_t index,
    uint8_t *output,
    uint32_t capacity,
    uint16_t requested_length,
    const AVPPlatformIdentity *identity,
    bool recovery,
    bool source_image_executed
) {
    static const uint8_t languages[] = {0x04, 0x03, 0x09, 0x04};
    static const char dfu_mode[] = "Apple Mobile Device (DFU Mode)";
    static const char recovery_mode[] = "Apple Mobile Device (Recovery Mode)";
    if (index == 0) {
        const uint32_t size = minimum(
            minimum((uint32_t)sizeof(languages), requested_length),
            capacity
        );
        for (uint32_t byte = 0; byte < size; ++byte) {
            output[byte] = languages[byte];
        }
        return size;
    }
    if (index == 1) {
        return emit_nonce_string(output, capacity, requested_length, identity);
    }
    if (index == 2) {
        return emit_ascii_string(
            output,
            capacity,
            "Apple Inc.",
            10,
            requested_length
        );
    }
    if (index == 3) {
        const uint32_t length = text_length(
            identity->variant,
            sizeof(identity->variant)
        );
        return emit_ascii_string(
            output,
            capacity,
            length == 0 ? dfu_mode : identity->variant,
            length == 0 ? sizeof(dfu_mode) - 1 : length,
            requested_length
        );
    }
    if (index == 4) {
        return emit_serial(
            output,
            capacity,
            requested_length,
            identity,
            source_image_executed
        );
    }
    if (index == 5) {
        return emit_ascii_string(
            output,
            capacity,
            recovery ? recovery_mode : dfu_mode,
            recovery ? sizeof(recovery_mode) - 1 : sizeof(dfu_mode) - 1,
            requested_length
        );
    }
    return UINT32_MAX;
}

#include "avp/devices/usb/usb_identity.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>


static uint32_t decode_string(
    const uint8_t *descriptor,
    uint32_t size,
    char *output,
    uint32_t capacity
) {
    assert(size >= 2);
    assert(descriptor[0] == size);
    assert(descriptor[1] == 3);
    const uint32_t length = (size - 2) / 2;
    assert(length < capacity);
    for (uint32_t index = 0; index < length; ++index) {
        assert(descriptor[3 + index * 2] == 0);
        output[index] = (char)descriptor[2 + index * 2];
    }
    output[length] = '\0';
    return length;
}

int main(void) {
    AVPPlatformIdentity identity = {
        .ecid = 0x1a2b3c4d5e6f7081,
        .chip_id = 0xfe01,
        .security_domain = 1,
        .chip_revision = 0,
        .chip_fuse_mode = 3,
        .security_epoch = 1,
        .board_id = 0x90,
        .boot_flags = 0x3c,
        .variant = "vresearch101",
    };
    for (uint32_t index = 0; index < sizeof(identity.ap_nonce); ++index) {
        identity.ap_nonce[index] = (uint8_t)index;
    }
    for (uint32_t index = 0; index < sizeof(identity.sep_nonce); ++index) {
        identity.sep_nonce[index] = (uint8_t)(0xa0 + index);
    }

    assert(usb_identity_product_id(&identity, false) == 0x1227);
    assert(usb_identity_product_id(&identity, true) == 0x1281);

    uint8_t descriptor[256] = {};
    char text[192] = {};
    uint32_t size = usb_identity_emit_string(
        1,
        descriptor,
        sizeof(descriptor),
        UINT16_MAX,
        &identity,
        false,
        false
    );
    assert(size == 254);
    assert(decode_string(descriptor, size, text, sizeof(text)) == 126);
    assert(strcmp(
        text,
        " NONC:000102030405060708090A0B0C0D0E0F1011121314151617"
        "18191A1B1C1D1E1F SNON:A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
        "B0B1B2B3 DCC:0:0:0"
    ) == 0);

    size = usb_identity_emit_string(
        3,
        descriptor,
        sizeof(descriptor),
        UINT16_MAX,
        &identity,
        false,
        false
    );
    decode_string(descriptor, size, text, sizeof(text));
    assert(strcmp(text, "vresearch101") == 0);

    size = usb_identity_emit_string(
        4,
        descriptor,
        sizeof(descriptor),
        UINT16_MAX,
        &identity,
        false,
        false
    );
    decode_string(descriptor, size, text, sizeof(text));
    assert(strcmp(
        text,
        "SDOM:01 CPID:FE01 CPRV:00 CPFM:03 SCEP:01 BDID:90 "
        "ECID:1A2B3C4D5E6F7081 IBFL:3C SRTG:[mBoot-20457.0.13.0.11]"
    ) == 0);

    size = usb_identity_emit_string(
        4,
        descriptor,
        sizeof(descriptor),
        UINT16_MAX,
        &identity,
        true,
        true
    );
    decode_string(descriptor, size, text, sizeof(text));
    assert(strstr(text, " IBSS:EXECUTED") != 0);
}

#include "avp/devices/simplevirtio/simple_virtio.h"

#include <assert.h>


int main(void) {
    const SimpleVirtioBlock block = {
        .base = SIMPLE_VIRTIO_ROOT_BLOCK_BASE,
        .sector_count = 128,
        .block_size = 4096,
    };
    SimpleVirtioSegment descriptors[3];
    SimpleVirtioBlockHeader header;
    uint8_t status;
    simple_virtio_block_build_request(
        &block,
        descriptors,
        &header,
        &status,
        0x71000000,
        7,
        1,
        false
    );
    assert(header.type == 0);
    assert(header.reserved == 0);
    assert(header.sector == 56);
    assert(status == UINT8_MAX);
    assert(descriptors[0].size == sizeof(header));
    assert(descriptors[0].flags == SIMPLE_VIRTIO_NEXT);
    assert(descriptors[1].ipa == 0x71000000);
    assert(descriptors[1].size == 0x1000);
    assert(descriptors[1].flags ==
        (SIMPLE_VIRTIO_WRITABLE | SIMPLE_VIRTIO_NEXT));
    assert(descriptors[2].size == 1);
    assert(descriptors[2].flags == SIMPLE_VIRTIO_WRITABLE);

    simple_virtio_block_build_request(
        &block,
        descriptors,
        &header,
        &status,
        0x71000000,
        3,
        2,
        true
    );
    assert(header.type == 1);
    assert(header.sector == 24);
    assert(descriptors[1].size == 8192);
    assert(descriptors[1].flags == SIMPLE_VIRTIO_NEXT);
}

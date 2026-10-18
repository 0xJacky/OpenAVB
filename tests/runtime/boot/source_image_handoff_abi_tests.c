#include "avp/image/boot/source_image.h"

#include <assert.h>
#include <stdio.h>

typedef void SourceImageHandoffSignature(
    uintptr_t image_base,
    uint32_t image_size,
    uintptr_t entry,
    uint32_t boot_mode
);

_Static_assert(
    __builtin_types_compatible_p(
        __typeof__(source_image_handoff),
        SourceImageHandoffSignature
    ),
    "source_image_handoff ABI"
);

int main(void) {
    assert(SOURCE_IMAGE_BOOT_MODE_NORMAL == 0);
    assert(SOURCE_IMAGE_BOOT_MODE_DFU == 2);
    puts("source image handoff ABI tests passed");
    return 0;
}

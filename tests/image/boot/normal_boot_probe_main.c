#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"


__attribute__((used, section(".source_image_tail")))
static const uint32_t normal_boot_payload_marker = 0x4e4f524d;

_Noreturn void source_ibss_main(void) {
    platform_stage(0x4e4f524d);
    pvpanic_trigger();
}

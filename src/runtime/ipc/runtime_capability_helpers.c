#include "avp/runtime/ipc/runtime_capability_helpers.h"


/*
 * Related public capability models:
 * FreeBSD 3aac283 Capsicum cap_rights_init (BSD-2-Clause):
 * https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
 * CHERIoT 923e4a0 capability model (MIT):
 * https://github.com/CHERIoT-Platform/cheriot-rtos/commit/923e4a061ec8f2cda5b322981f13b108aa5d85b9
 * AVP's packed entry, external audience mask, and four-register bounded tuple
 * are AVPBooter-compatible ABIs, so this owner reuses the local registry
 * implementation.
 */

#if defined(__ELF__)
#define RUNTIME_CAPABILITY_HELPERS_STATE \
    __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_CAPABILITY_HELPERS_STATE
#endif

static uint16_t enabled_external_audience_mask
    RUNTIME_CAPABILITY_HELPERS_STATE;
static RuntimeCapabilityHelpersPlatform capability_helpers_platform;
static bool capability_helpers_platform_bound;
static const uint16_t allowed_external_audience_mask =
    RUNTIME_CAPABILITY_ALLOWED_EXTERNAL_AUDIENCE_MASK;

static const RuntimeFirebloomType external_audience_mask_type = {
    .identity = UINT64_C(0x6175646d61736b),
    .element_size = sizeof(uint16_t),
    .bytewise_copyable = false,
};

const RuntimeFirebloomType runtime_type6_object_type = {
    .identity = UINT64_C(0x74797065366f626a),
    .element_size = RUNTIME_TYPE6_OBJECT_SIZE,
    .bytewise_copyable = false,
};

static _Noreturn void runtime_capability_helpers_fail(void) {
    __builtin_trap();
}

static bool runtime_capability_helpers_types_match(
    const RuntimeFirebloomType *left,
    const RuntimeFirebloomType *right
) {
    return left == right ||
        (left != 0 && right != 0 && left->identity != 0 &&
            left->identity == right->identity);
}

void runtime_capability_helpers_bind_platform(
    const RuntimeCapabilityHelpersPlatform *platform
) {
    if (platform == 0 || platform->classify == 0) {
        runtime_capability_helpers_fail();
    }
    capability_helpers_platform = *platform;
    capability_helpers_platform_bound = true;
}

void runtime_capability_helpers_reset_external_audience(void) {
    enabled_external_audience_mask = 0;
}

RuntimeFirebloomSpan
runtime_capability_allowed_external_audience_mask_span(void) {
    return (RuntimeFirebloomSpan){
        .current = (void *)&allowed_external_audience_mask,
        .lower_bound = (void *)&allowed_external_audience_mask,
        .upper_bound = (void *)(&allowed_external_audience_mask + 1),
        .type = &external_audience_mask_type,
    };
}

bool runtime_capability_external_audience_bit_is_enabled(uint8_t bit) {
    if (bit >= 16) {
        runtime_capability_helpers_fail();
    }
    return ((enabled_external_audience_mask >> bit) & UINT16_C(1)) != 0;
}

uint16_t runtime_object_publish_32(RuntimeFirebloomSpan object) {
    return runtime_object_publish(object, 0x20);
}

uintptr_t runtime_capability_validate_address_identity(uintptr_t address) {
    runtime_capability_validate_address(address);
    return address;
}

RuntimeFirebloomSpan runtime_type6_object_tuple_validate(
    RuntimeFirebloomSpan object
) {
    const uintptr_t current = (uintptr_t)object.current;
    const uintptr_t lower = (uintptr_t)object.lower_bound;
    const uintptr_t upper = (uintptr_t)object.upper_bound;
    if (object.type == 0 || current < lower || current >= upper ||
        upper - current < RUNTIME_TYPE6_OBJECT_SIZE ||
        object.type->element_size != RUNTIME_TYPE6_OBJECT_SIZE ||
        !runtime_capability_helpers_types_match(
            object.type,
            &runtime_type6_object_type
        )) {
        runtime_capability_helpers_fail();
    }
    object.type = &runtime_type6_object_type;
    return object;
}

bool runtime_object_address_is_admissible(
    uintptr_t address,
    size_t size
) {
    if (address == 0 || size == 0 || size > UINTPTR_MAX - address ||
        !runtime_capability_address_is_canonical(address)) {
        return false;
    }
    /* Memory ownership policy remains centralized in its own owner. */
    if (!capability_helpers_platform_bound) {
        runtime_capability_helpers_fail();
    }
    const uint8_t address_class = capability_helpers_platform.classify(
        capability_helpers_platform.context,
        address,
        size
    );
    return address_class == RUNTIME_OBJECT_ADDRESS_CLASS_PLATFORM ||
        (address_class == RUNTIME_OBJECT_ADDRESS_CLASS_RUNTIME_PRIVATE &&
            (address & (uintptr_t)0xf) == 0);
}

bool runtime_capability_enable_external_audience(uint8_t bit) {
    if (bit >= 16) {
        runtime_capability_helpers_fail();
    }
    enabled_external_audience_mask |= (uint16_t)(UINT16_C(1) << bit);
    return enabled_external_audience_mask ==
        RUNTIME_CAPABILITY_ALLOWED_EXTERNAL_AUDIENCE_MASK;
}

_Noreturn void runtime_capability_publish_zero_rights_fatal(void) {
    runtime_capability_helpers_fail();
}

void *runtime_type6_resolve_wait_signal_capability_from_x23(
    uint16_t handle
) {
    return runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_OBJECT,
        RUNTIME_CAPABILITY_RIGHT_WAIT | RUNTIME_CAPABILITY_RIGHT_SIGNAL
    );
}

bool runtime_capability_outlined_compare_entry_type_to_7(
    const uint32_t *entry
) {
    if (entry == 0) {
        runtime_capability_helpers_fail();
    }
    return (*entry >> 28) == RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING;
}

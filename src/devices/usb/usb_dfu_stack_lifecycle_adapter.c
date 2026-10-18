#include "avp/devices/usb/usb_dfu_stack_lifecycle_adapter.h"

#include "device/usbd.h"
#include "tusb.h"

#include <limits.h>


USBDFUStackPolicySpan usb_dfu_stack_policy_word_span(
    USBDFUStackLifecycle *stack
) {
    return (USBDFUStackPolicySpan){
        .current = &stack->policy_word,
        .lower_bound = (uintptr_t)&stack->policy_word,
        .upper_bound = (uintptr_t)&stack->policy_word +
            sizeof(stack->policy_word),
        .type = USB_DFU_STACK_POLICY_ACTIVE,
    };
}

void usb_dfu_stack_set_policy_bit16(
    USBDFUStackLifecycle *stack,
    bool enabled
) {
    if (enabled) {
        stack->policy_word |= USB_DFU_STACK_POLICY_ACTIVE;
    } else {
        stack->policy_word &= ~USB_DFU_STACK_POLICY_ACTIVE;
    }
}

bool usb_device_registry_read_required_byte(
    const USBDFUStackRegistry *registry,
    uint32_t key,
    uint8_t *value
) {
    for (size_t index = 0; index < registry->entry_count; ++index) {
        if (registry->entries[index].key == key) {
            *value = registry->entries[index].value;
            return true;
        }
    }
    return false;
}

void usb_device_local_command_0x11_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
) {
    stack->command_0x11 = value;
}

void usb_device_local_command_0x1a_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
) {
    stack->command_0x1a = value;
}

void usb_device_local_command_0x1f_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
) {
    stack->command_0x1f = value;
}

bool usb_device_dispatch_configuration_initialize_once(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
) {
    if (stack->configuration_initialized != 0) {
        return true;
    }

    uint8_t command_0x1a;
    uint8_t command_0x1f;
    if (!usb_device_registry_read_required_byte(
            registry,
            USB_DFU_STACK_REGISTRY_COMMAND_0X1A,
            &command_0x1a
        ) ||
        !usb_device_registry_read_required_byte(
            registry,
            USB_DFU_STACK_REGISTRY_COMMAND_0X1F,
            &command_0x1f
        )) {
        return false;
    }

    usb_device_local_command_0x11_set_byte(stack, 0);
    usb_device_local_command_0x1a_set_byte(stack, command_0x1a);
    usb_device_local_command_0x1f_set_byte(stack, command_0x1f);
    stack->configuration_initialized = 1;
    return true;
}

bool usb_device_transport_initialize(USBDFUStackLifecycle *stack) {
    if (stack->transport_initialized != 0) {
        return true;
    }
    const tusb_rhport_init_t rhport = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_HIGH,
    };
    if (!tusb_rhport_init(0, &rhport)) {
        return false;
    }
    stack->transport_initialized = 1;
    return true;
}

bool usb_dfu_commit_descriptors_for_rpc_transport(
    USBDFUStackLifecycle *stack
) {
    if (stack->transport_initialized == 0 || !tud_connect()) {
        return false;
    }
    stack->descriptors_committed = 1;
    return true;
}

bool usb_device_local_initialize(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
) {
    return usb_device_dispatch_configuration_initialize_once(
        stack,
        registry
    ) && usb_device_transport_initialize(stack);
}

bool usb_device_local_deinitialize(USBDFUStackLifecycle *stack) {
    if (stack->descriptors_committed != 0 && !tud_disconnect()) {
        return false;
    }
    stack->descriptors_committed = 0;
    if (stack->transport_initialized != 0 && !tusb_deinit(0)) {
        return false;
    }
    stack->transport_initialized = 0;
    return true;
}

int usb_dfu_stack_acquire(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
) {
    if (stack->lifecycle_busy != 0) {
        return -1;
    }
    stack->lifecycle_busy = 1;

    if (stack->reference_count != 0) {
        if (stack->reference_count != UINT32_MAX) {
            ++stack->reference_count;
        }
        usb_dfu_stack_set_policy_bit16(stack, true);
        stack->lifecycle_busy = 0;
        return 0;
    }

    if (!usb_device_local_initialize(stack, registry) ||
        !usb_dfu_commit_descriptors_for_rpc_transport(stack)) {
        if (stack->transport_initialized != 0) {
            (void)usb_device_local_deinitialize(stack);
        }
        stack->lifecycle_busy = 0;
        return -1;
    }

    stack->reference_count = 1;
    usb_dfu_stack_set_policy_bit16(stack, true);
    stack->lifecycle_busy = 0;
    return 0;
}

int usb_dfu_stack_acquire_tail_thunk(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
) {
    return usb_dfu_stack_acquire(stack, registry);
}

bool usb_dfu_stack_release(USBDFUStackLifecycle *stack) {
    if (stack->lifecycle_busy != 0 || stack->reference_count == 0) {
        return false;
    }
    stack->lifecycle_busy = 1;
    --stack->reference_count;
    if (stack->reference_count == 0) {
        usb_dfu_stack_set_policy_bit16(stack, false);
        if (!usb_device_local_deinitialize(stack)) {
            stack->lifecycle_busy = 0;
            return false;
        }
    }
    stack->lifecycle_busy = 0;
    return true;
}

void usb_dfu_device_lifecycle_state_noop(USBDFUStackLifecycle *stack) {
    (void)stack;
}

void usb_dfu_device_shutdown_local_noop(USBDFUStackLifecycle *stack) {
    (void)stack;
}

uint64_t usb_dfu_stack_guard_load_to_x8(
    const USBDFUStackLifecycle *stack
) {
    return stack->stack_guard;
}

bool usb_dfu_outlined_compare_policy_span_current_to_upper(
    USBDFUStackPolicySpan span
) {
    return (uintptr_t)span.current < span.upper_bound;
}

USBDFUStackPolicySpan usb_dfu_outlined_prepare_policy_word_span(
    USBDFUStackLifecycle *stack
) {
    return usb_dfu_stack_policy_word_span(stack);
}

bool usb_dfu_stack_guard_compare_saved_x8(
    const USBDFUStackLifecycle *stack,
    uint64_t saved_guard
) {
    return stack->stack_guard == saved_guard;
}

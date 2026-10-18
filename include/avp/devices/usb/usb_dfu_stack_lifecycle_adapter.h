#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DFU_STACK_LIFECYCLE_PUBLIC __attribute__((used, retain))

enum {
    USB_DFU_STACK_POLICY_ACTIVE = 1u << 16,
    USB_DFU_STACK_REGISTRY_COMMAND_0X1A = 0x02008900,
    USB_DFU_STACK_REGISTRY_COMMAND_0X1F = 0x02008f00,
};

typedef struct {
    uint32_t key;
    uint8_t value;
} USBDFUStackRegistryEntry;

typedef struct {
    const USBDFUStackRegistryEntry *entries;
    size_t entry_count;
} USBDFUStackRegistry;

typedef struct {
    uint32_t *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBDFUStackPolicySpan;

typedef struct {
    uint32_t policy_word;
    uint32_t reference_count;
    uint64_t stack_guard;
    uint8_t command_0x11;
    uint8_t command_0x1a;
    uint8_t command_0x1f;
    uint8_t configuration_initialized;
    uint8_t transport_initialized;
    uint8_t descriptors_committed;
    uint8_t lifecycle_busy;
} USBDFUStackLifecycle;

_Static_assert(sizeof(USBDFUStackPolicySpan) == 0x20, "policy span size");

USB_DFU_STACK_LIFECYCLE_PUBLIC USBDFUStackPolicySpan
usb_dfu_stack_policy_word_span(USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC void usb_dfu_stack_set_policy_bit16(
    USBDFUStackLifecycle *stack,
    bool enabled
);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool
usb_device_dispatch_configuration_initialize_once(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool usb_dfu_stack_release(
    USBDFUStackLifecycle *stack
);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool usb_device_transport_initialize(
    USBDFUStackLifecycle *stack
);

USB_DFU_STACK_LIFECYCLE_PUBLIC int usb_dfu_stack_acquire_tail_thunk(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
);

USB_DFU_STACK_LIFECYCLE_PUBLIC int usb_dfu_stack_acquire(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
);

USB_DFU_STACK_LIFECYCLE_PUBLIC void
usb_dfu_device_lifecycle_state_noop(USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC void
usb_dfu_device_shutdown_local_noop(USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool
usb_dfu_commit_descriptors_for_rpc_transport(USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool usb_device_registry_read_required_byte(
    const USBDFUStackRegistry *registry,
    uint32_t key,
    uint8_t *value
);

USB_DFU_STACK_LIFECYCLE_PUBLIC uint64_t
usb_dfu_stack_guard_load_to_x8(const USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool
usb_dfu_outlined_compare_policy_span_current_to_upper(
    USBDFUStackPolicySpan span
);

USB_DFU_STACK_LIFECYCLE_PUBLIC USBDFUStackPolicySpan
usb_dfu_outlined_prepare_policy_word_span(USBDFUStackLifecycle *stack);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool
usb_dfu_stack_guard_compare_saved_x8(
    const USBDFUStackLifecycle *stack,
    uint64_t saved_guard
);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool usb_device_local_deinitialize(
    USBDFUStackLifecycle *stack
);

USB_DFU_STACK_LIFECYCLE_PUBLIC bool usb_device_local_initialize(
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry
);

USB_DFU_STACK_LIFECYCLE_PUBLIC void usb_device_local_command_0x11_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
);

USB_DFU_STACK_LIFECYCLE_PUBLIC void usb_device_local_command_0x1a_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
);

USB_DFU_STACK_LIFECYCLE_PUBLIC void usb_device_local_command_0x1f_set_byte(
    USBDFUStackLifecycle *stack,
    uint8_t value
);

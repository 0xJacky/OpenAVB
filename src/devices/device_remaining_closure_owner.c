#include "avp/devices/device_remaining_closure_owner.h"

#include "avp/runtime/memory/runtime_memory.h"


void device_remaining_closure_owner_init(
    DeviceRemainingClosureOwner *owner,
    const SimpleVirtioAdapterIO *usb_io,
    uintptr_t usb_base,
    uint32_t configuration_offset,
    uint64_t stack_guard
) {
    *owner = (DeviceRemainingClosureOwner){
        .usb_io = *usb_io,
        .usb_base = usb_base,
        .configuration_offset = configuration_offset,
        .stack_guard = stack_guard,
    };
}

_Noreturn void simple_virtio_usb_endpoint_slot_fatal(void) {
    runtime_firebloom_panic();
}

SimpleVirtioAdapterStatus simple_virtio_usb_local_initialize(
    DeviceRemainingClosureOwner *owner
) {
    const SimpleVirtioAdapterStatus status = simplevirtio_device_initialize(
        &owner->usb_device,
        &owner->usb_io,
        owner->usb_base,
        SIMPLE_VIRTIO_USB_DEVICE_IDENTIFIER,
        0,
        false,
        0
    );
    if (status != SIMPLEVIRTIO_ADAPTER_OK) {
        return status;
    }
    owner->usb_io.write32(
        owner->usb_io.context,
        owner->usb_base,
        owner->configuration_offset,
        1
    );
    owner->usb_initialized = 1;
    owner->configuration_enabled = 1;
    ++owner->local_initialize_count;
    return SIMPLEVIRTIO_ADAPTER_OK;
}

bool sep_runtime_transport_stack_guard_compare_x8(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
) {
    return owner->stack_guard == saved_guard;
}

bool sep_runtime_stack_guard_compare(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
) {
    return owner->stack_guard == saved_guard;
}

uint64_t sep_runtime_stack_guard_load(
    const DeviceRemainingClosureOwner *owner
) {
    return owner->stack_guard;
}

int j_sep_bootstrap_configuration_exchange(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
) {
    return sep_bootstrap_configuration_exchange(session, result);
}

SEPDescriptorAddressResult sep_descriptor_address_add_checked(
    uintptr_t descriptor_offset
) {
    const uintptr_t address = descriptor_offset + SEP_DESCRIPTOR_TABLE_BASE;
    return (SEPDescriptorAddressResult){
        .address = address,
        .carry = address < descriptor_offset,
    };
}

SimpleVirtioClosureDeviceView simplevirtio_device_bounded_view(
    SimpleVirtioClosureDevice *device
) {
    return (SimpleVirtioClosureDeviceView){
        .current = device,
        .lower_bound = (uintptr_t)device,
        .upper_bound = (uintptr_t)device + sizeof(*device),
        .type = SIMPLE_VIRTIO_DEVICE_TYPE_IDENTITY,
    };
}

bool simple_virtio_storage_stack_guard_compare_saved_x8(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
) {
    return owner->stack_guard == saved_guard;
}

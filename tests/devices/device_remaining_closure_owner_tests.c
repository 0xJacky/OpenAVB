#include "avp/devices/device_remaining_closure_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>


enum {
    TEST_USB_BASE = 0x30100000,
    TEST_CONFIGURATION_OFFSET = 0x40c,
};

static struct {
    uint32_t version;
    uint32_t device_id;
    uint32_t features;
    uint32_t status;
    uint32_t configuration;
    uint32_t last_offset;
    uint32_t last_value;
    unsigned read_calls;
    unsigned write_calls;
} mmio_probe;

static struct {
    unsigned execute_calls;
} sep_probe;

static jmp_buf fatal_environment;
static volatile int fatal_observed;

_Noreturn void runtime_firebloom_panic(void) {
    fatal_observed = 1;
    longjmp(fatal_environment, 1);
}

static uint32_t read32(
    void *context,
    uintptr_t base,
    uint32_t offset
) {
    assert(context == &mmio_probe);
    assert(base == TEST_USB_BASE);
    ++mmio_probe.read_calls;
    switch (offset) {
        case 0x000:
            return mmio_probe.version;
        case 0x004:
            return mmio_probe.device_id;
        case 0x400:
            return mmio_probe.features;
        case 0x404:
            return mmio_probe.status;
        case TEST_CONFIGURATION_OFFSET:
            return mmio_probe.configuration;
        default:
            return 0;
    }
}

static void write32(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint32_t value
) {
    assert(context == &mmio_probe);
    assert(base == TEST_USB_BASE);
    ++mmio_probe.write_calls;
    mmio_probe.last_offset = offset;
    mmio_probe.last_value = value;
    switch (offset) {
        case 0x400:
            mmio_probe.features = value;
            break;
        case 0x404:
            mmio_probe.status = value;
            break;
        case TEST_CONFIGURATION_OFFSET:
            mmio_probe.configuration = value;
            break;
        default:
            break;
    }
}

static void write64(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint64_t value
) {
    assert(context == &mmio_probe);
    assert(base == TEST_USB_BASE);
    (void)offset;
    (void)value;
}

static uint64_t translate(void *context, uint64_t address) {
    assert(context == &mmio_probe);
    return address;
}

static int execute_sep(
    void *context,
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    uint32_t *responses,
    uint32_t response_capacity,
    SEPMailboxMessage *last_message
) {
    assert(context == &sep_probe);
    assert(initial_index == 0);
    assert(payload == 0);
    assert(extended_payload == 0);
    ++sep_probe.execute_calls;
    if (command_index == SEP_COMMAND_BOOTSTRAP) {
        assert(response_capacity == 1);
        responses[0] = SEP_PROTOCOL_BOOTSTRAP_PAYLOAD;
    } else {
        assert(command_index == SEP_COMMAND_CONFIGURATION);
        assert(response_capacity == 5);
        for (uint32_t index = 0; index < response_capacity; ++index) {
            responses[index] = index + 1;
        }
    }
    *last_message = (SEPMailboxMessage){0, 0};
    return 0;
}

static SimpleVirtioAdapterIO test_io(void) {
    return (SimpleVirtioAdapterIO){
        .context = &mmio_probe,
        .read32 = read32,
        .write32 = write32,
        .write64 = write64,
        .translate = translate,
    };
}

static void test_usb_local_initialize(void) {
    memset(&mmio_probe, 0, sizeof(mmio_probe));
    mmio_probe.version = 1;
    mmio_probe.device_id = SIMPLE_VIRTIO_USB_DEVICE_IDENTIFIER;
    const SimpleVirtioAdapterIO io = test_io();
    DeviceRemainingClosureOwner owner;
    device_remaining_closure_owner_init(
        &owner,
        &io,
        TEST_USB_BASE,
        TEST_CONFIGURATION_OFFSET,
        0x1122334455667788
    );
    assert(simple_virtio_usb_local_initialize(&owner) ==
        SIMPLEVIRTIO_ADAPTER_OK);
    assert(owner.usb_initialized == 1);
    assert(owner.configuration_enabled == 1);
    assert(owner.local_initialize_count == 1);
    assert(mmio_probe.features == 1);
    assert(owner.usb_device.status == 1);
    assert(mmio_probe.status == 0);
    assert(mmio_probe.configuration == 1);
    assert(mmio_probe.last_offset == TEST_CONFIGURATION_OFFSET);
    assert(mmio_probe.last_value == 1);

    mmio_probe.version = 2;
    assert(simple_virtio_usb_local_initialize(&owner) ==
        SIMPLEVIRTIO_ADAPTER_ERROR_VERSION);
    assert(owner.local_initialize_count == 1);
    assert(mmio_probe.configuration == 1);
}

static void test_sep_closure(void) {
    DeviceRemainingClosureOwner owner = {
        .stack_guard = UINT64_C(0x8899aabbccddeeff),
    };
    assert(sep_runtime_stack_guard_load(&owner) == owner.stack_guard);
    assert(sep_runtime_transport_stack_guard_compare_x8(
        &owner,
        owner.stack_guard
    ));
    assert(sep_runtime_stack_guard_compare(&owner, owner.stack_guard));
    assert(!sep_runtime_stack_guard_compare(&owner, owner.stack_guard + 1));
    assert(simple_virtio_storage_stack_guard_compare_saved_x8(
        &owner,
        owner.stack_guard
    ));
    assert(!simple_virtio_storage_stack_guard_compare_saved_x8(
        &owner,
        owner.stack_guard + 1
    ));

    const SEPDescriptorAddressResult address =
        sep_descriptor_address_add_checked(0x70);
    assert(address.address == SEP_DESCRIPTOR_TABLE_BASE + 0x70);
    assert(!address.carry);
    const SEPDescriptorAddressResult overflow =
        sep_descriptor_address_add_checked(UINTPTR_MAX);
    assert(overflow.address == SEP_DESCRIPTOR_TABLE_BASE - 1);
    assert(overflow.carry);

    memset(&sep_probe, 0, sizeof(sep_probe));
    SEPProtocolHelperSession session = {
        .execute = execute_sep,
        .context = &sep_probe,
    };
    SEPBootstrapResult result;
    assert(j_sep_bootstrap_configuration_exchange(&session, &result) ==
        SEP_PROTOCOL_HELPER_OK);
    assert(sep_probe.execute_calls == 2);
    assert(result.bootstrap_payload == SEP_PROTOCOL_BOOTSTRAP_PAYLOAD);
    assert(result.configuration_words[4] == 5);
}

static void test_simplevirtio_bounded_view(void) {
    SimpleVirtioClosureDevice device;
    const SimpleVirtioClosureDeviceView view =
        simplevirtio_device_bounded_view(&device);
    assert(view.current == &device);
    assert(view.lower_bound == (uintptr_t)&device);
    assert(view.upper_bound - view.lower_bound ==
        SIMPLE_VIRTIO_DEVICE_VIEW_SIZE);
    assert(view.type == SIMPLE_VIRTIO_DEVICE_TYPE_IDENTITY);
}

static void test_endpoint_slot_fatal(void) {
    fatal_observed = 0;
    if (setjmp(fatal_environment) == 0) {
        simple_virtio_usb_endpoint_slot_fatal();
    }
    assert(fatal_observed == 1);
}

int main(void) {
    test_usb_local_initialize();
    test_sep_closure();
    test_simplevirtio_bounded_view();
    test_endpoint_slot_fatal();
    puts("device remaining closure owner tests passed");
    return 0;
}

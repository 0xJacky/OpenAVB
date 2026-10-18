#include "avp/devices/usb/usb_dfu_stack_lifecycle_adapter.h"

#include "tusb.h"

#include <assert.h>
#include <stdio.h>


static unsigned tinyusb_initialize_calls;
static unsigned tinyusb_deinitialize_calls;
static unsigned tinyusb_connect_calls;
static unsigned tinyusb_disconnect_calls;
static bool tinyusb_initialize_result = true;
static bool tinyusb_deinitialize_result = true;
static bool tinyusb_connect_result = true;
static bool tinyusb_disconnect_result = true;

bool tusb_rhport_init(
    uint8_t rhport,
    const tusb_rhport_init_t *configuration
) {
    assert(rhport == 0);
    assert(configuration->role == TUSB_ROLE_DEVICE);
    assert(configuration->speed == TUSB_SPEED_HIGH);
    ++tinyusb_initialize_calls;
    return tinyusb_initialize_result;
}

bool tusb_deinit(uint8_t rhport) {
    assert(rhport == 0);
    ++tinyusb_deinitialize_calls;
    return tinyusb_deinitialize_result;
}

bool tud_connect(void) {
    ++tinyusb_connect_calls;
    return tinyusb_connect_result;
}

bool tud_disconnect(void) {
    ++tinyusb_disconnect_calls;
    return tinyusb_disconnect_result;
}

static USBDFUStackRegistry test_registry(void) {
    static const USBDFUStackRegistryEntry entries[] = {
        {USB_DFU_STACK_REGISTRY_COMMAND_0X1A, 0x2a},
        {USB_DFU_STACK_REGISTRY_COMMAND_0X1F, 0x3f},
    };
    return (USBDFUStackRegistry){
        .entries = entries,
        .entry_count = sizeof(entries) / sizeof(entries[0]),
    };
}

static void reset_tinyusb_transcript(void) {
    tinyusb_initialize_calls = 0;
    tinyusb_deinitialize_calls = 0;
    tinyusb_connect_calls = 0;
    tinyusb_disconnect_calls = 0;
    tinyusb_initialize_result = true;
    tinyusb_deinitialize_result = true;
    tinyusb_connect_result = true;
    tinyusb_disconnect_result = true;
}

static void test_policy_span_and_outliners(void) {
    USBDFUStackLifecycle stack = {.stack_guard = UINT64_C(0x12345678)};
    USBDFUStackPolicySpan span = usb_dfu_stack_policy_word_span(&stack);
    assert(span.current == &stack.policy_word);
    assert(span.lower_bound == (uintptr_t)&stack.policy_word);
    assert(span.upper_bound - span.lower_bound == sizeof(stack.policy_word));
    assert(span.type == USB_DFU_STACK_POLICY_ACTIVE);
    assert(usb_dfu_outlined_compare_policy_span_current_to_upper(span));

    USBDFUStackPolicySpan prepared =
        usb_dfu_outlined_prepare_policy_word_span(&stack);
    assert(prepared.current == span.current);
    assert(prepared.lower_bound == span.lower_bound);
    assert(prepared.upper_bound == span.upper_bound);
    assert(usb_dfu_stack_guard_load_to_x8(&stack) == UINT64_C(0x12345678));
    assert(usb_dfu_stack_guard_compare_saved_x8(
        &stack,
        UINT64_C(0x12345678)
    ));
    assert(!usb_dfu_stack_guard_compare_saved_x8(&stack, 0));

    usb_dfu_stack_set_policy_bit16(&stack, true);
    assert((stack.policy_word & USB_DFU_STACK_POLICY_ACTIVE) != 0);
    usb_dfu_stack_set_policy_bit16(&stack, false);
    assert((stack.policy_word & USB_DFU_STACK_POLICY_ACTIVE) == 0);
}

static void test_registry_and_configuration_once(void) {
    USBDFUStackLifecycle stack = {0};
    const USBDFUStackRegistry registry = test_registry();
    uint8_t value = 0;
    assert(usb_device_registry_read_required_byte(
        &registry,
        USB_DFU_STACK_REGISTRY_COMMAND_0X1A,
        &value
    ));
    assert(value == 0x2a);
    assert(!usb_device_registry_read_required_byte(
        &registry,
        0xffffffff,
        &value
    ));

    assert(usb_device_dispatch_configuration_initialize_once(
        &stack,
        &registry
    ));
    assert(stack.command_0x11 == 0);
    assert(stack.command_0x1a == 0x2a);
    assert(stack.command_0x1f == 0x3f);
    assert(stack.configuration_initialized == 1);

    usb_device_local_command_0x1a_set_byte(&stack, 0xaa);
    assert(usb_device_dispatch_configuration_initialize_once(
        &stack,
        &registry
    ));
    assert(stack.command_0x1a == 0xaa);
}

static void test_acquire_release_transcript(void) {
    reset_tinyusb_transcript();
    USBDFUStackLifecycle stack = {0};
    const USBDFUStackRegistry registry = test_registry();

    assert(usb_dfu_stack_acquire_tail_thunk(&stack, &registry) == 0);
    assert(stack.reference_count == 1);
    assert(stack.transport_initialized == 1);
    assert(stack.descriptors_committed == 1);
    assert((stack.policy_word & USB_DFU_STACK_POLICY_ACTIVE) != 0);
    assert(tinyusb_initialize_calls == 1);
    assert(tinyusb_connect_calls == 1);

    assert(usb_dfu_stack_acquire(&stack, &registry) == 0);
    assert(stack.reference_count == 2);
    assert(tinyusb_initialize_calls == 1);
    assert(tinyusb_connect_calls == 1);

    assert(usb_dfu_stack_release(&stack));
    assert(stack.reference_count == 1);
    assert(tinyusb_disconnect_calls == 0);
    assert(usb_dfu_stack_release(&stack));
    assert(stack.reference_count == 0);
    assert(stack.transport_initialized == 0);
    assert(stack.descriptors_committed == 0);
    assert((stack.policy_word & USB_DFU_STACK_POLICY_ACTIVE) == 0);
    assert(tinyusb_disconnect_calls == 1);
    assert(tinyusb_deinitialize_calls == 1);
    assert(!usb_dfu_stack_release(&stack));
}

static void test_failed_transport_is_observable(void) {
    reset_tinyusb_transcript();
    tinyusb_connect_result = false;
    USBDFUStackLifecycle stack = {0};
    const USBDFUStackRegistry registry = test_registry();
    assert(usb_dfu_stack_acquire(&stack, &registry) == -1);
    assert(stack.reference_count == 0);
    assert(stack.lifecycle_busy == 0);
    assert(stack.transport_initialized == 0);
    assert(tinyusb_initialize_calls == 1);
    assert(tinyusb_connect_calls == 1);
    assert(tinyusb_deinitialize_calls == 1);
}

static void test_lifecycle_noop_contracts(void) {
    USBDFUStackLifecycle stack = {.policy_word = 0xa5a5a5a5};
    usb_dfu_device_lifecycle_state_noop(&stack);
    usb_dfu_device_shutdown_local_noop(&stack);
    assert(stack.policy_word == 0xa5a5a5a5);
}

int main(void) {
    test_policy_span_and_outliners();
    test_registry_and_configuration_once();
    test_acquire_release_transcript();
    test_failed_transport_is_observable();
    test_lifecycle_noop_contracts();
    puts("usb_dfu_stack_lifecycle_adapter_tests: ok");
    return 0;
}

#include "avp/runtime/boot/boot_device_closure_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>


enum {
    EVENT_MODE = 1,
    EVENT_SHUTDOWN = 2,
    EVENT_PROGRESS = 3,
    EVENT_POLICY = 4,
};

typedef struct {
    uint32_t kind;
    uint32_t value;
} Event;

static struct {
    Event events[16];
    uint32_t event_count;
    uint32_t finalize_calls;
    bool flag19_accepted;
} probe;

static jmp_buf fatal_environment;
static volatile int fatal_observed;

bool tusb_rhport_init(
    uint8_t rhport,
    const tusb_rhport_init_t *configuration
) {
    (void)rhport;
    (void)configuration;
    return true;
}

bool tusb_deinit(uint8_t rhport) {
    assert(rhport == 0);
    return true;
}

bool tud_connect(void) {
    return true;
}

bool tud_disconnect(void) {
    return true;
}

bool tud_control_status(
    uint8_t rhport,
    const tusb_control_request_t *request
) {
    (void)rhport;
    (void)request;
    return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t endpoint) {
    (void)rhport;
    (void)endpoint;
}

_Noreturn void runtime_firebloom_panic(void) {
    fatal_observed = 1;
    longjmp(fatal_environment, 1);
}

static void append_event(uint32_t kind, uint32_t value) {
    assert(probe.event_count < 16);
    probe.events[probe.event_count++] = (Event){kind, value};
}

static void shutdown(void *context) {
    assert(context == &probe);
    append_event(EVENT_SHUTDOWN, 0);
}

static void progress(void *context, uint32_t code) {
    assert(context == &probe);
    append_event(EVENT_PROGRESS, code);
}

static void mode_transition(void *context, uint32_t marker, bool active) {
    assert(context == &probe);
    assert(!active);
    append_event(EVENT_MODE, marker);
}

static bool accept_policy_flags(void *context, uint32_t flags) {
    assert(context == &probe);
    append_event(EVENT_POLICY, flags);
    if (flags == BOOT_DEVICE_POLICY_FLAG19) {
        return probe.flag19_accepted;
    }
    return true;
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
    assert(context == &probe);
    assert(command_index == SEP_COMMAND_FINALIZE);
    assert(initial_index == 0);
    assert(payload == 0);
    assert(extended_payload == 0);
    assert(responses == NULL);
    assert(response_capacity == 0);
    ++probe.finalize_calls;
    *last_message = (SEPMailboxMessage){
        .low = UINT64_C(0x65) << 16,
        .high = 0,
    };
    return 0;
}

static BootDeviceClosureOperations operations(void) {
    return (BootDeviceClosureOperations){
        .context = &probe,
        .shutdown = shutdown,
        .progress = progress,
        .mode_transition = mode_transition,
        .accept_policy_flags = accept_policy_flags,
    };
}

static void test_dfu_policy_atoms(void) {
    uint8_t bytes[32];
    USBDeviceByteSpan span = usb_device_make_byte_span(
        bytes,
        bytes + sizeof(bytes)
    );
    assert(boot_dispatch_validate_dfu_download_span(span));
    span.type = 0;
    assert(!boot_dispatch_validate_dfu_download_span(span));
    assert(platform_set_dfu_mode_noop(false) == 0);
    assert(j_platform_set_dfu_mode_noop(true) == 0);
    assert(platform_boot_dfu_image_type_ibss() == UINT32_C(0x69627373));
    assert(boot_platform_prepare_sep_shutdown_noop() == 0);

    BootDeviceClosureOwner owner;
    USBDFUStackLifecycle stack = {0};
    SEPProtocolHelperSession session = {0};
    SEPBootstrapResult result = {0};
    boot_device_closure_owner_init(
        &owner,
        &stack,
        &session,
        &result,
        operations(),
        0x15
    );
    assert(platform_claim_force_dfu(&owner));
    assert(owner.boot_options == 0x14);
    assert(!platform_claim_force_dfu(&owner));
}

static void test_usb_platform_finalize_transcript(void) {
    memset(&probe, 0, sizeof(probe));
    USBDFUStackLifecycle stack = {
        .policy_word = USB_DFU_STACK_POLICY_ACTIVE,
        .reference_count = 1,
    };
    SEPProtocolHelperSession session = {0};
    SEPBootstrapResult result = {0};
    BootDeviceClosureOwner owner;
    boot_device_closure_owner_init(
        &owner,
        &stack,
        &session,
        &result,
        operations(),
        0
    );
    assert(boot_finalize_usb_and_platform_services(&owner) == 0);
    assert(owner.usb_release_observed == 1);
    assert(owner.platform_shutdown_observed == 1);
    assert(stack.reference_count == 0);
    assert((stack.policy_word & USB_DFU_STACK_POLICY_ACTIVE) == 0);
    assert(probe.event_count == 3);
    assert(probe.events[0].kind == EVENT_MODE);
    assert(probe.events[0].value == BOOT_DEVICE_FINAL_PROGRESS);
    assert(probe.events[1].kind == EVENT_SHUTDOWN);
    assert(probe.events[2].kind == EVENT_PROGRESS);
    assert(probe.events[2].value == BOOT_DEVICE_FINAL_PROGRESS);
}

static void test_sep_platform_finalize_transcript(void) {
    memset(&probe, 0, sizeof(probe));
    USBDFUStackLifecycle stack = {0};
    SEPProtocolHelperSession session = {
        .execute = execute_sep,
        .context = &probe,
    };
    SEPBootstrapResult result = {0};
    BootDeviceClosureOwner owner;
    boot_device_closure_owner_init(
        &owner,
        &stack,
        &session,
        &result,
        operations(),
        0
    );
    owner.progress_words[0] = 0x11223344;
    owner.progress_words[1] = 0x55667788;
    assert(boot_finalize_sep_and_platform_state(&owner, 2) == 0);
    assert(owner.sep_finalize_observed == 1);
    assert(result.finalize_rounds == SEP_PROTOCOL_FINALIZE_ROUNDS);
    assert(probe.finalize_calls == SEP_PROTOCOL_FINALIZE_ROUNDS);
    assert(owner.progress_words[0] == 0);
    assert(owner.progress_words[1] == 0);
    assert(probe.event_count == 4);
    assert(probe.events[0].value == BOOT_DEVICE_MODE2_ENTER);
    assert(probe.events[1].value == BOOT_DEVICE_MODE2_EXIT);
    assert(probe.events[2].value == BOOT_DEVICE_POLICY_FLAG18);
    assert(probe.events[3].value == BOOT_DEVICE_POLICY_FLAG19);
}

static void test_invalid_mode_is_observable(void) {
    memset(&probe, 0, sizeof(probe));
    USBDFUStackLifecycle stack = {0};
    SEPProtocolHelperSession session = {0};
    SEPBootstrapResult result = {0};
    BootDeviceClosureOwner owner;
    boot_device_closure_owner_init(
        &owner,
        &stack,
        &session,
        &result,
        operations(),
        0
    );
    owner.mode3_state = -1;
    fatal_observed = 0;
    if (setjmp(fatal_environment) == 0) {
        (void)boot_finalize_sep_and_platform_state(&owner, 3);
    }
    assert(fatal_observed == 1);
    assert(owner.mode3_state == 0x7f);
}

int main(void) {
    test_dfu_policy_atoms();
    test_usb_platform_finalize_transcript();
    test_sep_platform_finalize_transcript();
    test_invalid_mode_is_observable();
    puts("boot device closure owner tests passed");
    return 0;
}

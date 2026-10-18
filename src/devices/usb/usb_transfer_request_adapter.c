#include "avp/devices/usb/usb_transfer_request_adapter.h"

#include "common/tusb_common.h"
#include "device/usbd_pvt.h"


static bool buffer_span_contains(
    USBTransferBufferSpan span,
    size_t required_length
) {
    if (required_length == 0 && span.current == 0 &&
        span.lower_bound == 0 && span.upper_bound == 0) {
        return true;
    }
    const uintptr_t current = (uintptr_t)span.current;
    return span.type == USB_DESCRIPTOR_SPAN_TYPE_BYTE &&
        span.lower_bound <= current && current <= span.upper_bound &&
        required_length <= span.upper_bound - current;
}

static void clear_bytes(void *object, size_t size) {
    uint8_t *bytes = object;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static bool pool_release(
    USBTransferRequestPool *pool,
    USBTransferRequest *request
) {
    const uintptr_t current = (uintptr_t)request;
    const uintptr_t lower = (uintptr_t)pool->records;
    const uintptr_t upper = lower + pool->capacity * sizeof(*pool->records);
    if (current < lower || current >= upper ||
        (current - lower) % sizeof(*pool->records) != 0) {
        return false;
    }
    const size_t index = (current - lower) / sizeof(*pool->records);
    if (pool->in_use[index] == 0) {
        return false;
    }
    pool->in_use[index] = 0;
    clear_bytes(request, sizeof(*request));
    return true;
}

USBTransferRequestSpan usb_transfer_request_allocate_one(
    USBTransferRequestPool *pool
) {
    for (size_t index = 0; index < pool->capacity; ++index) {
        if (pool->in_use[index] == 0) {
            USBTransferRequest *const request = &pool->records[index];
            clear_bytes(request, sizeof(*request));
            pool->in_use[index] = 1;
            return (USBTransferRequestSpan){
                .current = request,
                .lower_bound = (uintptr_t)request,
                .upper_bound = (uintptr_t)request + sizeof(*request),
                .type = USB_TRANSFER_REQUEST_TYPE,
            };
        }
    }
    return (USBTransferRequestSpan){0};
}

bool usb_transfer_request_validate_tuple(USBTransferRequestSpan request) {
    const uintptr_t current = (uintptr_t)request.current;
    return request.type == USB_TRANSFER_REQUEST_TYPE &&
        request.current != 0 && request.lower_bound <= current &&
        current < request.upper_bound &&
        sizeof(*request.current) <= request.upper_bound - current;
}

USBTransferRequestSpan usb_transfer_outlined_initialize_request_tail(
    USBTransferRequestSpan request,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
) {
    USBTransferRequest *const current = request.current;
    current->requested_length = requested_length;
    current->transfer_state = 0;
    current->completion = completion;
    current->completion_type = completion_type;
    current->next = (USBTransferRequestSpan){0};
    return request;
}

USBTransferRequestSpan usb_transfer_outlined_restore_request_span_x22_x19(
    USBTransferRequestSpan request
) {
    return request;
}

bool usb_transfer_request_complete_and_release(
    USBTransferRequestPool *pool,
    USBTransferRequestSpan request,
    USBTransferCompletionState *controller
) {
    if (!usb_transfer_request_validate_tuple(request)) {
        return false;
    }
    USBTransferRequest *const current = request.current;
    if (current->completion != 0) {
        current->completion(
            request.current,
            request.lower_bound,
            request.upper_bound,
            request.type
        );
    }
    if (current->endpoint_address == 0x80 &&
        controller->ep0_in_phase >= 0) {
        controller->ep0_in_completion = 1;
    }
    return pool_release(pool, current);
}

bool usb_transfer_request_create_and_submit(
    USBTransferRequestPool *pool,
    uint8_t endpoint_address,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type,
    USBTransferRequestSpan *submitted_request
) {
    *submitted_request = (USBTransferRequestSpan){0};
    if (requested_length > UINT16_MAX ||
        !buffer_span_contains(payload, requested_length)) {
        return false;
    }
    USBTransferRequestSpan request = usb_transfer_request_allocate_one(pool);
    if (!usb_transfer_request_validate_tuple(request)) {
        return false;
    }
    request.current->endpoint_address = endpoint_address;
    request.current->payload = payload;
    request = usb_transfer_outlined_initialize_request_tail(
        request,
        requested_length,
        completion,
        completion_type
    );
    if (!usbd_edpt_xfer(
        0,
        endpoint_address,
        payload.current,
        (uint16_t)requested_length,
        false
    )) {
        (void)pool_release(pool, request.current);
        return false;
    }
    *submitted_request = request;
    return true;
}

bool usb_ep0_validate_descriptor_span_thunk(USBDescriptorSpan descriptor) {
    return usb_configuration_descriptor_validate_tuple(descriptor);
}

USBTransferRequestSpan usb_ep0_outlined_clear_transfer_span(void) {
    return (USBTransferRequestSpan){0};
}

USBTransferSpanBounds usb_ep0_outlined_load_payload_bounds(
    const USBTransferEP0State *state
) {
    const uintptr_t current = (uintptr_t)state->payload.current;
    const uintptr_t lower = (uintptr_t)state->payload.lower_bound;
    return (USBTransferSpanBounds){
        .current = current,
        .lower_bound = lower,
        .upper_bound = (uintptr_t)state->payload.upper_bound,
        .current_at_or_above_lower = current >= lower,
    };
}

USBDescriptorSpan usb_ep0_outlined_load_payload_span(
    const USBTransferEP0State *state
) {
    return state->payload;
}

USBDescriptorSpan usb_ep0_outlined_setup_request_span_x25(
    USBTransferEP0State *state
) {
    uint8_t *const setup = (uint8_t *)&state->setup;
    return (USBDescriptorSpan){
        .current = setup,
        .lower_bound = setup,
        .upper_bound = setup + sizeof(state->setup),
        .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
    };
}

USBTransferCallbackSpan usb_ep0_outlined_load_endpoint_callback_span_x9(
    const USBTransferCallbackSpan *record
) {
    return *record;
}

USBTransferCallbackSpan *usb_ep0_outlined_select_endpoint_callback_record(
    USBTransferEP0State *state,
    uint32_t endpoint_index
) {
    if (endpoint_index >= USB_TRANSFER_CALLBACK_COUNT) {
        return 0;
    }
    return &state->endpoint_callbacks[endpoint_index];
}

USBTransferClampedLength
usb_ep0_outlined_clamp_configuration_descriptor_length(
    const tusb_desc_configuration_t *descriptor,
    uint16_t requested_length
) {
    const uint16_t total_length = tu_unaligned_read16(
        (const uint8_t *)descriptor + offsetof(
            tusb_desc_configuration_t,
            wTotalLength
        )
    );
    const uint16_t length = requested_length < total_length ?
        requested_length : total_length;
    return (USBTransferClampedLength){
        .length = length,
        .exceeds_control_limit = length > USB_EP0_CONTROL_LENGTH_LIMIT,
    };
}

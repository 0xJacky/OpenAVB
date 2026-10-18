#include "avp/devices/usb/usb_rpc_transport_adapter.h"


static USBRPCBuilderSpan builder_span(RuntimeRPCBuilder *builder) {
    return (USBRPCBuilderSpan){
        .current = builder,
        .lower_bound = builder,
        .upper_bound = (uintptr_t)builder + sizeof(*builder),
        .type_token = USB_RPC_BUILDER_TYPE_TOKEN,
    };
}

static RuntimeRPCPayloadSpan payload_span(
    const void *value,
    size_t size,
    uintptr_t type_token
) {
    return (RuntimeRPCPayloadSpan){
        .current = value,
        .lower_bound = value,
        .upper_bound = (const uint8_t *)value + size,
        .type_token = type_token,
    };
}

static RuntimeRPCFieldSpec field_spec(
    size_t size,
    uint64_t type_token
) {
    return (RuntimeRPCFieldSpec){
        .alignment = size,
        .size = size,
        .pointer_offset = 0,
        .type_token = type_token,
    };
}

void usb_rpc_outlined_builder_initialize_sp20_x24(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
    runtime_rpc_builder_initialize(builder, arena, arena_size);
}

USBRPCServiceCall usb_rpc_prepare_service_call(
    const RuntimeRPCBuilder *builder
) {
    return (USBRPCServiceCall){
        .service_id = USB_RPC_SERVICE_ID,
        .packet = runtime_rpc_builder_view(builder),
        .transfer_flag = builder->field_index,
    };
}

void usb_rpc_outlined_builder_initialize_sp20_x20(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
    runtime_rpc_builder_initialize(builder, arena, arena_size);
}

USBRPCAppendArguments usb_rpc_outlined_prepare_sp20_builder_field(
    RuntimeRPCBuilder *builder
) {
    return (USBRPCAppendArguments){
        .builder = builder_span(builder),
    };
}

bool usb_rpc_require_response_field_zero(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
) {
    const RuntimeRPCFieldHeader *required = runtime_rpc_field_require(
        packet,
        field,
        0
    );
    if (runtime_rpc_field_payload_size(required) != sizeof(uint32_t)) {
        return false;
    }
    return *(const uint32_t *)runtime_rpc_field_payload_const(required) == 0;
}

USBRPCBuilderSpan usb_rpc_outlined_sp20_builder_span_x24(
    RuntimeRPCBuilder *builder
) {
    return builder_span(builder);
}

RuntimeRPCFieldHeader *usb_rpc_append_empty_field_zero(
    RuntimeRPCBuilder *builder
) {
    return runtime_rpc_builder_append_field(
        builder,
        (RuntimeRPCFieldSpec){0},
        (RuntimeRPCPayloadSpan){0}
    );
}

USBRPCBuilderSpan usb_rpc_outlined_sp40_builder_span_x20(
    RuntimeRPCBuilder *builder
) {
    return builder_span(builder);
}

RuntimeRPCFieldHeader *usb_rpc_outlined_append_empty_field_zero_x20(
    RuntimeRPCBuilder *builder
) {
    return usb_rpc_append_empty_field_zero(builder);
}

RuntimeRPCPayloadSpan usb_rpc_outlined_save_field_span_x20_x23(
    RuntimeRPCPayloadSpan field
) {
    return field;
}

USBRPCAppendArguments usb_rpc_outlined_prepare_empty_u32_field(
    RuntimeRPCBuilder *builder
) {
    return (USBRPCAppendArguments){
        .builder = builder_span(builder),
        .field = field_spec(sizeof(uint32_t), 0),
    };
}

USBRPCAppendArguments usb_rpc_outlined_prepare_sp28_byte_field_arguments(
    RuntimeRPCBuilder *builder,
    const uint8_t *value
) {
    return usb_rpc_outlined_prepare_single_byte_field(
        builder,
        value,
        AVP_RUNTIME_RPC_NO_POINTER_TOKEN
    );
}

USBRPCBuilderSpan usb_rpc_outlined_sp28_builder_span_x20(
    RuntimeRPCBuilder *builder
) {
    return builder_span(builder);
}

USBRPCServiceCall usb_rpc_outlined_prepare_sp28_service_call(
    const RuntimeRPCBuilder *builder
) {
    return usb_rpc_prepare_service_call(builder);
}

USBRPCAppendArguments usb_rpc_outlined_prepare_single_byte_field(
    RuntimeRPCBuilder *builder,
    const uint8_t *value,
    uint64_t field_type
) {
    return (USBRPCAppendArguments){
        .builder = builder_span(builder),
        .field = field_spec(sizeof(*value), field_type),
        .payload = payload_span(value, sizeof(*value), field_type),
    };
}

bool usb_rpc_require_sp28_response_field_zero(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
) {
    return usb_rpc_require_response_field_zero(packet, field);
}

USBRPCBuilderSpan usb_rpc_outlined_sp40_builder_span_x25(
    RuntimeRPCBuilder *builder
) {
    return builder_span(builder);
}

RuntimeRPCFieldSpec usb_rpc_outlined_prepare_u32_field(
    uint64_t field_type
) {
    return field_spec(sizeof(uint32_t), field_type);
}

RuntimeRPCFieldHeader *usb_rpc_outlined_append_u64_field_sp40(
    RuntimeRPCBuilder *builder,
    const uint64_t *value,
    uint64_t field_type,
    uintptr_t payload_type
) {
    return runtime_rpc_builder_append_field(
        builder,
        field_spec(sizeof(*value), field_type),
        payload_span(value, sizeof(*value), payload_type)
    );
}

void usb_rpc_outlined_initialize_sp28_builder_arguments(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
    runtime_rpc_builder_initialize(builder, arena, arena_size);
}

RuntimeRPCFieldSpec usb_rpc_outlined_prepare_single_byte_field_lengths(
    uint64_t field_type
) {
    return field_spec(sizeof(uint8_t), field_type);
}

RuntimeRPCFieldSpec usb_rpc_outlined_prepare_u16_field(
    uint64_t field_type
) {
    return field_spec(sizeof(uint16_t), field_type);
}

uintptr_t usb_rpc_outlined_load_builder_type_x19(void) {
    return USB_RPC_BUILDER_TYPE_TOKEN;
}

USBRPCBuilderSpan usb_rpc_outlined_sp28_builder_current_lower(
    RuntimeRPCBuilder *builder
) {
    return builder_span(builder);
}

void usb_rpc_outlined_clear_sp28_builder(RuntimeRPCBuilder *builder) {
    *builder = (RuntimeRPCBuilder){0};
}

uint64_t usb_rpc_outlined_select_field_type_x7(
    uint64_t candidate,
    uint64_t default_type
) {
    return candidate == 0 ? default_type : candidate;
}

USBRPCStagedSource
usb_rpc_outlined_stage_source_span_and_load_builder_type_x23(
    RuntimeRPCPayloadSpan source
) {
    return (USBRPCStagedSource){
        .source = source,
        .builder_type_token = USB_RPC_BUILDER_TYPE_TOKEN,
    };
}

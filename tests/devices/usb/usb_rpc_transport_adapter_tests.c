#include "avp/devices/usb/usb_rpc_transport_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


_Noreturn void runtime_firebloom_panic(void) {
    abort();
}

void *runtime_memmove(void *destination, const void *source, size_t size) {
    return memmove(destination, source, size);
}

void runtime_bzero(void *destination, size_t size) {
    memset(destination, 0, size);
}

static void assert_builder_span(
    USBRPCBuilderSpan span,
    RuntimeRPCBuilder *builder
) {
    assert(span.current == builder);
    assert(span.lower_bound == builder);
    assert(span.upper_bound == (uintptr_t)builder + 0x28);
    assert(span.type_token == USB_RPC_BUILDER_TYPE_TOKEN);
}

static void test_builder_initializers_and_spans(void) {
    uint64_t arena_a[64];
    uint64_t arena_b[64];
    uint64_t arena_c[64];
    RuntimeRPCBuilder builder_a;
    RuntimeRPCBuilder builder_b;
    RuntimeRPCBuilder builder_c;

    usb_rpc_outlined_builder_initialize_sp20_x24(
        &builder_a,
        arena_a,
        sizeof(arena_a)
    );
    usb_rpc_outlined_builder_initialize_sp20_x20(
        &builder_b,
        arena_b,
        sizeof(arena_b)
    );
    usb_rpc_outlined_initialize_sp28_builder_arguments(
        &builder_c,
        arena_c,
        sizeof(arena_c)
    );
    assert(arena_a[0] == AVP_RUNTIME_RPC_PACKET_MAGIC);
    assert(arena_b[0] == AVP_RUNTIME_RPC_PACKET_MAGIC);
    assert(arena_c[0] == AVP_RUNTIME_RPC_PACKET_MAGIC);

    assert_builder_span(
        usb_rpc_outlined_sp20_builder_span_x24(&builder_a),
        &builder_a
    );
    assert_builder_span(
        usb_rpc_outlined_sp40_builder_span_x20(&builder_a),
        &builder_a
    );
    assert_builder_span(
        usb_rpc_outlined_sp28_builder_span_x20(&builder_a),
        &builder_a
    );
    assert_builder_span(
        usb_rpc_outlined_sp40_builder_span_x25(&builder_a),
        &builder_a
    );
    assert_builder_span(
        usb_rpc_outlined_sp28_builder_current_lower(&builder_a),
        &builder_a
    );
    assert_builder_span(
        usb_rpc_outlined_prepare_sp20_builder_field(&builder_a).builder,
        &builder_a
    );
    assert(usb_rpc_outlined_load_builder_type_x19() ==
        USB_RPC_BUILDER_TYPE_TOKEN);

    usb_rpc_outlined_clear_sp28_builder(&builder_c);
    const uint8_t *bytes = (const uint8_t *)&builder_c;
    for (size_t index = 0; index < sizeof(builder_c); ++index) {
        assert(bytes[index] == 0);
    }
}

static void test_field_argument_outliners(void) {
    RuntimeRPCBuilder builder = {0};
    const uint8_t byte = 0x5a;
    const uint64_t field_type = AVP_RUNTIME_RPC_NO_POINTER_TOKEN;

    USBRPCAppendArguments empty =
        usb_rpc_outlined_prepare_empty_u32_field(&builder);
    assert(empty.field.alignment == 4);
    assert(empty.field.size == 4);
    assert(empty.field.type_token == 0);
    assert(empty.payload.current == 0);

    USBRPCAppendArguments byte_args =
        usb_rpc_outlined_prepare_sp28_byte_field_arguments(&builder, &byte);
    assert(byte_args.field.alignment == 1);
    assert(byte_args.field.size == 1);
    assert(byte_args.field.type_token == field_type);
    assert(byte_args.payload.current == &byte);

    byte_args = usb_rpc_outlined_prepare_single_byte_field(
        &builder,
        &byte,
        field_type
    );
    assert(byte_args.payload.upper_bound == &byte + 1);

    RuntimeRPCFieldSpec spec = usb_rpc_outlined_prepare_u32_field(field_type);
    assert(spec.alignment == 4 && spec.size == 4 && spec.pointer_offset == 0);
    spec = usb_rpc_outlined_prepare_u16_field(field_type);
    assert(spec.alignment == 2 && spec.size == 2 && spec.pointer_offset == 0);
    spec = usb_rpc_outlined_prepare_single_byte_field_lengths(field_type);
    assert(spec.alignment == 1 && spec.size == 1 && spec.pointer_offset == 0);

    assert(usb_rpc_outlined_select_field_type_x7(0, field_type) == field_type);
    assert(usb_rpc_outlined_select_field_type_x7(0x1234, field_type) == 0x1234);

    const RuntimeRPCPayloadSpan span = {
        .current = &byte,
        .lower_bound = &byte,
        .upper_bound = &byte + 1,
        .type_token = 0x144f00,
    };
    const RuntimeRPCPayloadSpan saved =
        usb_rpc_outlined_save_field_span_x20_x23(span);
    assert(memcmp(&saved, &span, sizeof(span)) == 0);
    const USBRPCStagedSource staged =
        usb_rpc_outlined_stage_source_span_and_load_builder_type_x23(span);
    assert(memcmp(&staged.source, &span, sizeof(span)) == 0);
    assert(staged.builder_type_token == USB_RPC_BUILDER_TYPE_TOKEN);
}

static void test_packet_transcript_and_response(void) {
    uint64_t arena[64];
    RuntimeRPCBuilder builder;
    usb_rpc_outlined_builder_initialize_sp20_x24(
        &builder,
        arena,
        sizeof(arena)
    );

    RuntimeRPCFieldHeader *response = usb_rpc_append_empty_field_zero(&builder);
    const uint64_t metadata = 0x123456789abcdef0;
    RuntimeRPCFieldHeader *metadata_field =
        usb_rpc_outlined_append_u64_field_sp40(
            &builder,
            &metadata,
            AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
            0x144f00
        );
    assert(metadata_field != 0);
    const uint64_t *metadata_payload = runtime_rpc_field_payload_const(
        metadata_field
    );
    assert(*metadata_payload == metadata);
    const size_t byte_size = runtime_rpc_builder_finalize(&builder);

    USBRPCServiceCall call = usb_rpc_prepare_service_call(&builder);
    assert(call.service_id == USB_RPC_SERVICE_ID);
    assert(call.packet.base == arena);
    assert(call.packet.byte_size == byte_size);
    assert(call.transfer_flag == 2);
    USBRPCServiceCall sp28_call =
        usb_rpc_outlined_prepare_sp28_service_call(&builder);
    assert(memcmp(&call, &sp28_call, sizeof(call)) == 0);

    assert(!usb_rpc_require_response_field_zero(call.packet, response));
}

static void test_zero_response_contract(void) {
    uint64_t arena[32];
    RuntimeRPCBuilder builder;
    usb_rpc_outlined_builder_initialize_sp20_x20(
        &builder,
        arena,
        sizeof(arena)
    );
    USBRPCAppendArguments empty =
        usb_rpc_outlined_prepare_empty_u32_field(&builder);
    RuntimeRPCFieldHeader *field = runtime_rpc_builder_append_field(
        &builder,
        empty.field,
        empty.payload
    );
    (void)runtime_rpc_builder_finalize(&builder);
    const RuntimeRPCPacketView packet = runtime_rpc_builder_view(&builder);
    assert(usb_rpc_require_response_field_zero(packet, field));
    assert(usb_rpc_require_sp28_response_field_zero(packet, field));

    *(uint32_t *)runtime_rpc_field_payload(field) = 7;
    assert(!usb_rpc_require_response_field_zero(packet, field));
}

static void test_empty_field_clone(void) {
    uint64_t arena[32];
    RuntimeRPCBuilder builder;
    usb_rpc_outlined_initialize_sp28_builder_arguments(
        &builder,
        arena,
        sizeof(arena)
    );
    RuntimeRPCFieldHeader *field =
        usb_rpc_outlined_append_empty_field_zero_x20(&builder);
    assert(field->magic == AVP_RUNTIME_RPC_FIELD_MAGIC);
    assert(field->size_and_flags == 0);
}

int main(void) {
    test_builder_initializers_and_spans();
    test_field_argument_outliners();
    test_packet_transcript_and_response();
    test_zero_response_contract();
    test_empty_field_clone();
    puts("usb_rpc_transport_adapter_tests: ok");
    return 0;
}

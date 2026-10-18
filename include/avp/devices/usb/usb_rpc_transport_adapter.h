#pragma once

#include "avp/runtime/ipc/runtime_rpc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_RPC_TRANSPORT_PUBLIC __attribute__((used, retain))

enum {
    USB_RPC_SERVICE_ID = 0x627375,
    USB_RPC_BUILDER_TYPE_TOKEN = 0x1474d0,
};

typedef struct {
    RuntimeRPCBuilder *current;
    RuntimeRPCBuilder *lower_bound;
    uintptr_t upper_bound;
    uintptr_t type_token;
} USBRPCBuilderSpan;

_Static_assert(sizeof(USBRPCBuilderSpan) == 0x20, "USB RPC builder span size");

typedef struct {
    USBRPCBuilderSpan builder;
    RuntimeRPCFieldSpec field;
    RuntimeRPCPayloadSpan payload;
} USBRPCAppendArguments;

typedef struct {
    uint32_t service_id;
    RuntimeRPCPacketView packet;
    uint8_t transfer_flag;
} USBRPCServiceCall;

typedef struct {
    RuntimeRPCPayloadSpan source;
    uintptr_t builder_type_token;
} USBRPCStagedSource;

USB_RPC_TRANSPORT_PUBLIC void usb_rpc_outlined_builder_initialize_sp20_x24(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
);

USB_RPC_TRANSPORT_PUBLIC USBRPCServiceCall usb_rpc_prepare_service_call(
    const RuntimeRPCBuilder *builder
);

USB_RPC_TRANSPORT_PUBLIC void usb_rpc_outlined_builder_initialize_sp20_x20(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
);

USB_RPC_TRANSPORT_PUBLIC USBRPCAppendArguments
usb_rpc_outlined_prepare_sp20_builder_field(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC bool usb_rpc_require_response_field_zero(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
);

USB_RPC_TRANSPORT_PUBLIC USBRPCBuilderSpan
usb_rpc_outlined_sp20_builder_span_x24(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldHeader *
usb_rpc_append_empty_field_zero(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC USBRPCBuilderSpan
usb_rpc_outlined_sp40_builder_span_x20(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldHeader *
usb_rpc_outlined_append_empty_field_zero_x20(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCPayloadSpan
usb_rpc_outlined_save_field_span_x20_x23(RuntimeRPCPayloadSpan field);

USB_RPC_TRANSPORT_PUBLIC USBRPCAppendArguments
usb_rpc_outlined_prepare_empty_u32_field(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC USBRPCAppendArguments
usb_rpc_outlined_prepare_sp28_byte_field_arguments(
    RuntimeRPCBuilder *builder,
    const uint8_t *value
);

USB_RPC_TRANSPORT_PUBLIC USBRPCBuilderSpan
usb_rpc_outlined_sp28_builder_span_x20(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC USBRPCServiceCall
usb_rpc_outlined_prepare_sp28_service_call(
    const RuntimeRPCBuilder *builder
);

USB_RPC_TRANSPORT_PUBLIC USBRPCAppendArguments
usb_rpc_outlined_prepare_single_byte_field(
    RuntimeRPCBuilder *builder,
    const uint8_t *value,
    uint64_t field_type
);

USB_RPC_TRANSPORT_PUBLIC bool usb_rpc_require_sp28_response_field_zero(
    RuntimeRPCPacketView packet,
    const RuntimeRPCFieldHeader *field
);

USB_RPC_TRANSPORT_PUBLIC USBRPCBuilderSpan
usb_rpc_outlined_sp40_builder_span_x25(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldSpec
usb_rpc_outlined_prepare_u32_field(uint64_t field_type);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldHeader *
usb_rpc_outlined_append_u64_field_sp40(
    RuntimeRPCBuilder *builder,
    const uint64_t *value,
    uint64_t field_type,
    uintptr_t payload_type
);

USB_RPC_TRANSPORT_PUBLIC void
usb_rpc_outlined_initialize_sp28_builder_arguments(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldSpec
usb_rpc_outlined_prepare_single_byte_field_lengths(uint64_t field_type);

USB_RPC_TRANSPORT_PUBLIC RuntimeRPCFieldSpec
usb_rpc_outlined_prepare_u16_field(uint64_t field_type);

USB_RPC_TRANSPORT_PUBLIC uintptr_t usb_rpc_outlined_load_builder_type_x19(void);

USB_RPC_TRANSPORT_PUBLIC USBRPCBuilderSpan
usb_rpc_outlined_sp28_builder_current_lower(RuntimeRPCBuilder *builder);

USB_RPC_TRANSPORT_PUBLIC void usb_rpc_outlined_clear_sp28_builder(
    RuntimeRPCBuilder *builder
);

USB_RPC_TRANSPORT_PUBLIC uint64_t usb_rpc_outlined_select_field_type_x7(
    uint64_t candidate,
    uint64_t default_type
);

USB_RPC_TRANSPORT_PUBLIC USBRPCStagedSource
usb_rpc_outlined_stage_source_span_and_load_builder_type_x23(
    RuntimeRPCPayloadSpan source
);

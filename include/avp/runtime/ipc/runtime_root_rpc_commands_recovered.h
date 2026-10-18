#pragma once

#include "avp/runtime/ipc/runtime_rpc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_ROOT_RPC_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_ROOT_RPC_PUBLIC __attribute__((used))
#endif

#define ROOT_RPC_SERVICE_ROOT UINT32_C(0x746f6f72)
#define ROOT_RPC_COMMAND_CONSOLE_WRITE UINT32_C(0x22)
#define ROOT_RPC_COMMAND_MEMORY_READABLE UINT32_C(0x26)
#define ROOT_RPC_BUILDER_TYPE_TOKEN UINT64_C(0x144f30)

typedef int (*RootRPCInvoke)(
  void *context,
  uint32_t service,
  uint32_t command,
  RuntimeRPCPacketView request,
  void *response_arena,
  size_t response_capacity,
  size_t *response_size
);

typedef struct {
  RootRPCInvoke invoke;
  void *context;
} RootRPCClient;

typedef struct {
  uint32_t service;
  uint32_t command;
  RuntimeRPCPacketView request;
} RootRPCServiceCall;

typedef struct {
  RuntimeRPCFieldSpec spec;
  RuntimeRPCPayloadSpan payload;
} RootRPCFieldArguments;

typedef struct {
  RuntimeRPCBuilder *builder;
  RuntimeRPCPacketView view;
  const void *source_current;
} RootRPCBuilderViewArguments;

typedef struct {
  RuntimeRPCBuilder *builder;
  uint64_t *lower_bound;
  uint64_t *upper_bound;
  uintptr_t type_token;
} RootRPCBuilderBounds;

typedef struct {
  uintptr_t saved_registers[10];
  uint8_t local_storage[0x70];
} RootRPCRegisterFrame;

_Static_assert(sizeof(RootRPCRegisterFrame) == 0xc0,
               "Root RPC recovered frame size");

RUNTIME_ROOT_RPC_PUBLIC uint32_t
root_rpc_console_write(
  RootRPCClient *client,
  RuntimeRPCPayloadSpan source,
  uint64_t channel,
  bool flush
);
RUNTIME_ROOT_RPC_PUBLIC bool
root_rpc_memory_span_is_readable(
  RootRPCClient *client,
  RuntimeRPCPayloadSpan source
);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_memory_outlined_stage_u64_field(const uint64_t *value);
RUNTIME_ROOT_RPC_PUBLIC RootRPCServiceCall
root_rpc_outlined_prepare_service_call(
  RuntimeRPCPacketView request,
  uint32_t command
);
RUNTIME_ROOT_RPC_PUBLIC bool
root_rpc_outlined_stack_guard_compare_saved_x8(uintptr_t saved);
RUNTIME_ROOT_RPC_PUBLIC RootRPCServiceCall
root_rpc_console_outlined_prepare_service_call(
  RuntimeRPCPacketView request,
  uint32_t command
);
RUNTIME_ROOT_RPC_PUBLIC RootRPCBuilderViewArguments
root_rpc_console_outlined_prepare_builder_view_and_source_current(
  RuntimeRPCBuilder *builder,
  const void *source_current
);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCPayloadSpan
root_rpc_memory_outlined_save_source_span_x20_x23(RuntimeRPCPayloadSpan source);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_prepare_u32_field_arguments(const uint32_t *value);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCBuilder *
root_rpc_outlined_initialize_sp28_builder_arguments(
  RuntimeRPCBuilder *builder,
  void *arena,
  size_t arena_size
);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCBuilder *
root_rpc_console_outlined_initialize_sp20_builder_arguments(
  RuntimeRPCBuilder *builder,
  void *arena,
  size_t arena_size
);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCPayloadSpan
root_rpc_console_outlined_restore_response_span_x4_x7(
    RuntimeRPCPayloadSpan response);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_memory_outlined_prepare_u64_field_arguments(const uint64_t *value);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCBuilder *
root_rpc_console_outlined_clear_sp20_builder(RuntimeRPCBuilder *builder);
RUNTIME_ROOT_RPC_PUBLIC uintptr_t
root_rpc_outlined_stack_guard_load_to_x8(void);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCFieldHeader *
root_rpc_lifecycle_outlined_append_empty_field_zero(RuntimeRPCBuilder *builder);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_outlined_prepare_single_byte_field(const uint8_t *value);
RUNTIME_ROOT_RPC_PUBLIC RootRPCBuilderBounds
root_rpc_outlined_sp28_builder_span_x21_with_type(
  RuntimeRPCBuilder *builder,
  uintptr_t type_token
);
RUNTIME_ROOT_RPC_PUBLIC const RuntimeRPCFieldHeader *
root_rpc_memory_outlined_require_response_field(
  RuntimeRPCPacketView response,
  uint8_t field_index
);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_prepare_u64_field_arguments(const uint64_t *value);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_stage_u64_stack_field(const uint64_t *value);
RUNTIME_ROOT_RPC_PUBLIC RootRPCBuilderBounds
root_rpc_outlined_sp28_builder_bounds_x21(RuntimeRPCBuilder *builder);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_prepare_byte_field_lengths(const uint8_t *value);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCBuilder *
root_rpc_console_outlined_clear_sp10_and_prepare_sp20_builder(
    RuntimeRPCBuilder *builder);
RUNTIME_ROOT_RPC_PUBLIC RuntimeRPCBuilder *
root_rpc_lifecycle_outlined_clear_builder_frame(RuntimeRPCBuilder *builder);
RUNTIME_ROOT_RPC_PUBLIC RootRPCBuilderBounds
root_rpc_outlined_sp28_builder_span_x28_with_type(
  RuntimeRPCBuilder *builder,
  uintptr_t type_token
);
RUNTIME_ROOT_RPC_PUBLIC uint64_t
root_rpc_console_outlined_load_is_not_ptr_tag_x24(void);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_stage_byte_stack_field(const uint8_t *value);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_console_outlined_save_builder_and_prepare_pointer_field(
  const RuntimeRPCPayloadSpan *source,
  uintptr_t type_token
);
RUNTIME_ROOT_RPC_PUBLIC uint64_t
root_rpc_memory_outlined_load_ptr_is_raw_tag_x7(void);
RUNTIME_ROOT_RPC_PUBLIC RootRPCFieldArguments
root_rpc_memory_outlined_prepare_32_byte_field_arguments(
  const RuntimeRPCPayloadSpan *source,
  uintptr_t type_token
);
RUNTIME_ROOT_RPC_PUBLIC uint64_t
root_rpc_console_outlined_load_builder_type_x23(void);
RUNTIME_ROOT_RPC_PUBLIC RootRPCRegisterFrame
root_rpc_outlined_prologue_save_x19_x28_allocate_c0(
    const uintptr_t registers[10]);
RUNTIME_ROOT_RPC_PUBLIC void
root_rpc_outlined_epilogue_restore_x19_x28_release_c0(
  const RootRPCRegisterFrame *frame,
  uintptr_t registers[10]
);

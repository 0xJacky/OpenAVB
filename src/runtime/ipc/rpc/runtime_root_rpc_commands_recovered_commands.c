#include "runtime_root_rpc_commands_recovered_private.h"

static size_t root_rpc_span_size(RuntimeRPCPayloadSpan span) {
  const uintptr_t current = (uintptr_t)span.current;
  const uintptr_t lower = (uintptr_t)span.lower_bound;
  const uintptr_t upper = (uintptr_t)span.upper_bound;
  if (span.type_token == 0 || current < lower || current > upper) {
    root_rpc_recovered_trap();
  }
  return upper - current;
}

static RuntimeRPCPacketView root_rpc_invoke(
    RootRPCClient *client,
    RootRPCServiceCall call,
    uint64_t *response_arena,
    size_t response_capacity
) {
  if (client == NULL || client->invoke == NULL) {
    root_rpc_recovered_trap();
  }
  size_t response_size = 0;
  if (client->invoke(client->context, call.service, call.command, call.request,
                     response_arena, response_capacity, &response_size) != 0 ||
      response_size > response_capacity) {
    root_rpc_recovered_trap();
  }
  return (RuntimeRPCPacketView){
      .base = response_arena,
      .byte_size = response_size,
  };
}

uint32_t RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_write(
    RootRPCClient *client,
    RuntimeRPCPayloadSpan source,
    uint64_t channel,
    bool flush
) {
  const uintptr_t saved_guard = root_rpc_outlined_stack_guard_load_to_x8();
  (void)root_rpc_span_size(source);
  uint64_t request_arena[128];
  uint64_t response_arena[64];
  RuntimeRPCBuilder builder;
  (void)root_rpc_console_outlined_initialize_sp20_builder_arguments(
      &builder,
      request_arena,
      sizeof(request_arena)
  );
  (void)root_rpc_lifecycle_outlined_append_empty_field_zero(&builder);

  RootRPCFieldArguments pointer =
      root_rpc_console_outlined_save_builder_and_prepare_pointer_field(
          &source,
          source.type_token
      );
  (void)runtime_rpc_builder_append_field(
      &builder,
      pointer.spec,
      pointer.payload
  );
  RootRPCFieldArguments channel_field =
      root_rpc_console_outlined_stage_u64_stack_field(&channel);
  (void)runtime_rpc_builder_append_field(
      &builder,
      channel_field.spec,
      channel_field.payload
  );
  const uint8_t flush_byte = flush ? 1 : 0;
  RootRPCFieldArguments flush_field =
      root_rpc_console_outlined_stage_byte_stack_field(&flush_byte);
  (void)runtime_rpc_builder_append_field(
      &builder,
      flush_field.spec,
      flush_field.payload
  );
  (void)runtime_rpc_builder_finalize(&builder);

  RootRPCServiceCall call = root_rpc_console_outlined_prepare_service_call(
      runtime_rpc_builder_view(&builder),
      ROOT_RPC_COMMAND_CONSOLE_WRITE
  );
  RuntimeRPCPacketView response =
      root_rpc_invoke(client, call, response_arena, sizeof(response_arena));
  const RuntimeRPCFieldHeader *field =
      root_rpc_memory_outlined_require_response_field(response, 0);
  if (runtime_rpc_field_payload_size(field) != sizeof(uint32_t)) {
    root_rpc_recovered_trap();
  }
  const uint32_t result =
      *(const uint32_t *)runtime_rpc_field_payload_const(field);
  if (!root_rpc_outlined_stack_guard_compare_saved_x8(saved_guard)) {
    root_rpc_recovered_trap();
  }
  return result;
}

bool RUNTIME_ROOT_RPC_PUBLIC root_rpc_memory_span_is_readable(
    RootRPCClient *client,
    RuntimeRPCPayloadSpan source
) {
  const uintptr_t saved_guard = root_rpc_outlined_stack_guard_load_to_x8();
  const uint64_t size = root_rpc_span_size(source);
  uint64_t request_arena[128];
  uint64_t response_arena[64];
  RuntimeRPCBuilder builder;
  (void)root_rpc_outlined_initialize_sp28_builder_arguments(
      &builder,
      request_arena,
      sizeof(request_arena)
  );
  (void)root_rpc_lifecycle_outlined_append_empty_field_zero(&builder);

  RuntimeRPCPayloadSpan saved =
      root_rpc_memory_outlined_save_source_span_x20_x23(source);
  RootRPCFieldArguments pointer =
      root_rpc_memory_outlined_prepare_32_byte_field_arguments(
          &saved,
          saved.type_token
      );
  (void)runtime_rpc_builder_append_field(
      &builder,
      pointer.spec,
      pointer.payload
  );
  RootRPCFieldArguments size_field =
      root_rpc_memory_outlined_stage_u64_field(&size);
  (void)runtime_rpc_builder_append_field(
      &builder,
      size_field.spec,
      size_field.payload
  );
  (void)runtime_rpc_builder_finalize(&builder);

  RootRPCServiceCall call = root_rpc_outlined_prepare_service_call(
      runtime_rpc_builder_view(&builder),
      ROOT_RPC_COMMAND_MEMORY_READABLE
  );
  RuntimeRPCPacketView response =
      root_rpc_invoke(client, call, response_arena, sizeof(response_arena));
  const RuntimeRPCFieldHeader *field =
      root_rpc_memory_outlined_require_response_field(response, 0);
  if (runtime_rpc_field_payload_size(field) != sizeof(uint8_t)) {
    root_rpc_recovered_trap();
  }
  const bool readable =
      *(const uint8_t *)runtime_rpc_field_payload_const(field) != 0;
  if (!root_rpc_outlined_stack_guard_compare_saved_x8(saved_guard)) {
    root_rpc_recovered_trap();
  }
  return readable;
}

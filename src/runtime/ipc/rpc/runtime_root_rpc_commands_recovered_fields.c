#include "runtime_root_rpc_commands_recovered_private.h"

extern uintptr_t __stack_chk_guard;

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_stage_u64_field(const uint64_t *value) {
  return root_rpc_scalar_arguments(value, sizeof(*value), _Alignof(uint64_t));
}

RootRPCServiceCall RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_prepare_service_call(
    RuntimeRPCPacketView request,
    uint32_t command
) {
  return (RootRPCServiceCall){
      .service = ROOT_RPC_SERVICE_ROOT,
      .command = command,
      .request = request,
  };
}

bool RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_stack_guard_compare_saved_x8(uintptr_t saved) {
  return saved == __stack_chk_guard;
}

RootRPCServiceCall RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_prepare_service_call(
    RuntimeRPCPacketView request,
    uint32_t command
) {
  return root_rpc_outlined_prepare_service_call(request, command);
}

RootRPCBuilderViewArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_prepare_builder_view_and_source_current(
    RuntimeRPCBuilder *builder,
    const void *source_current
) {
  return (RootRPCBuilderViewArguments){
      .builder = builder,
      .view = runtime_rpc_builder_view(builder),
      .source_current = source_current,
  };
}

RuntimeRPCPayloadSpan RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_save_source_span_x20_x23(
    RuntimeRPCPayloadSpan source) {
  return source;
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_prepare_u32_field_arguments(const uint32_t *value) {
  return root_rpc_scalar_arguments(value, sizeof(*value), _Alignof(uint32_t));
}

RuntimeRPCBuilder *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_initialize_sp28_builder_arguments(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
  runtime_rpc_builder_initialize(builder, arena, arena_size);
  return builder;
}

RuntimeRPCBuilder *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_initialize_sp20_builder_arguments(
    RuntimeRPCBuilder *builder,
    void *arena,
    size_t arena_size
) {
  return root_rpc_outlined_initialize_sp28_builder_arguments(
      builder,
      arena,
      arena_size
  );
}

RuntimeRPCPayloadSpan RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_restore_response_span_x4_x7(
    RuntimeRPCPayloadSpan response) {
  return response;
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_prepare_u64_field_arguments(const uint64_t *value) {
  return root_rpc_scalar_arguments(value, sizeof(*value), _Alignof(uint64_t));
}

RuntimeRPCBuilder *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_clear_sp20_builder(RuntimeRPCBuilder *builder) {
  *builder = (RuntimeRPCBuilder){0};
  return builder;
}

uintptr_t RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_stack_guard_load_to_x8(void) {
  return __stack_chk_guard;
}

RuntimeRPCFieldHeader *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_lifecycle_outlined_append_empty_field_zero(
    RuntimeRPCBuilder *builder) {
  return runtime_rpc_builder_append_field(
      builder,
      (RuntimeRPCFieldSpec){0},
      (RuntimeRPCPayloadSpan){0}
  );
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_prepare_single_byte_field(const uint8_t *value) {
  return root_rpc_scalar_arguments(value, sizeof(*value), _Alignof(uint8_t));
}

static RootRPCBuilderBounds root_rpc_builder_bounds(
    RuntimeRPCBuilder *builder,
    uintptr_t type_token
) {
  return (RootRPCBuilderBounds){
      .builder = builder,
      .lower_bound = builder->lower_bound,
      .upper_bound = builder->upper_bound,
      .type_token = type_token,
  };
}

RootRPCBuilderBounds RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_sp28_builder_span_x21_with_type(
    RuntimeRPCBuilder *builder,
    uintptr_t type_token
) {
  return root_rpc_builder_bounds(builder, type_token);
}

const RuntimeRPCFieldHeader *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_require_response_field(
    RuntimeRPCPacketView response,
    uint8_t field_index
) {
  const RuntimeRPCFieldHeader *field =
      runtime_rpc_field_find(response, field_index);
  if (field == NULL) {
    root_rpc_recovered_trap();
  }
  return runtime_rpc_field_require(response, field, field_index);
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_prepare_u64_field_arguments(const uint64_t *value) {
  return root_rpc_scalar_arguments(value, sizeof(*value), _Alignof(uint64_t));
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_stage_u64_stack_field(const uint64_t *value) {
  return root_rpc_console_outlined_prepare_u64_field_arguments(value);
}

RootRPCBuilderBounds RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_sp28_builder_bounds_x21(RuntimeRPCBuilder *builder) {
  return root_rpc_builder_bounds(builder, builder->arena_type_token);
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_prepare_byte_field_lengths(const uint8_t *value) {
  return root_rpc_outlined_prepare_single_byte_field(value);
}

RuntimeRPCBuilder *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_clear_sp10_and_prepare_sp20_builder(
    RuntimeRPCBuilder *builder) {
  return root_rpc_console_outlined_clear_sp20_builder(builder);
}

RuntimeRPCBuilder *RUNTIME_ROOT_RPC_PUBLIC
root_rpc_lifecycle_outlined_clear_builder_frame(RuntimeRPCBuilder *builder) {
  *builder = (RuntimeRPCBuilder){0};
  return builder;
}

RootRPCBuilderBounds RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_sp28_builder_span_x28_with_type(
    RuntimeRPCBuilder *builder,
    uintptr_t type_token
) {
  return root_rpc_builder_bounds(builder, type_token);
}

uint64_t RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_load_is_not_ptr_tag_x24(void) {
  return AVP_RUNTIME_RPC_NO_POINTER_TOKEN;
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_stage_byte_stack_field(const uint8_t *value) {
  return root_rpc_outlined_prepare_single_byte_field(value);
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_save_builder_and_prepare_pointer_field(
    const RuntimeRPCPayloadSpan *source,
    uintptr_t type_token
) {
  if (source == NULL || type_token == 0) {
    root_rpc_recovered_trap();
  }
  return (RootRPCFieldArguments){
      .spec =
          {
              .alignment = _Alignof(RuntimeRPCPayloadSpan),
              .size = sizeof(*source),
              .pointer_offset = 0,
              .type_token = type_token,
              .span = true,
          },
      .payload =
          {
              .current = source,
              .lower_bound = source,
              .upper_bound = source + 1,
              .type_token = type_token,
          },
  };
}

uint64_t RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_load_ptr_is_raw_tag_x7(void) {
  return AVP_RUNTIME_RPC_RAW_POINTER_TOKEN;
}

RootRPCFieldArguments RUNTIME_ROOT_RPC_PUBLIC
root_rpc_memory_outlined_prepare_32_byte_field_arguments(
    const RuntimeRPCPayloadSpan *source,
    uintptr_t type_token
) {
  return root_rpc_console_outlined_save_builder_and_prepare_pointer_field(
      source,
      type_token
  );
}

uint64_t RUNTIME_ROOT_RPC_PUBLIC
root_rpc_console_outlined_load_builder_type_x23(void) {
  return ROOT_RPC_BUILDER_TYPE_TOKEN;
}

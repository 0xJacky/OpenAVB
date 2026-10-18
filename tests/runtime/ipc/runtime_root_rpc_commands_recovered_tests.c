#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/ipc/runtime_root_rpc_commands_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

uintptr_t __stack_chk_guard = UINT64_C(0x123456789abcdef0);

_Noreturn void runtime_firebloom_panic(void) { __builtin_trap(); }

void *runtime_memmove(void *destination, const void *source, size_t size) {
  return memmove(destination, source, size);
}

void runtime_bzero(void *destination, size_t size) {
  (void)memset(destination, 0, size);
}

typedef struct {
  uint32_t calls;
  uint32_t last_command;
  uint64_t last_channel;
  uint64_t last_span_size;
  bool last_flush;
} TransportProbe;

static void build_response(void *arena, size_t capacity, const void *value,
                           size_t size, size_t alignment,
                           size_t *response_size) {
  RuntimeRPCBuilder builder;
  runtime_rpc_builder_initialize(&builder, arena, capacity);
  RuntimeRPCPayloadSpan payload = {
      .current = value,
      .lower_bound = value,
      .upper_bound = (const uint8_t *)value + size,
      .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
  };
  (void)runtime_rpc_builder_append_field(&builder,
                                         (RuntimeRPCFieldSpec){
                                             .alignment = alignment,
                                             .size = size,
                                         },
                                         payload);
  *response_size = runtime_rpc_builder_finalize(&builder);
}

static int transport_invoke(void *context, uint32_t service, uint32_t command,
                            RuntimeRPCPacketView request, void *response_arena,
                            size_t response_capacity, size_t *response_size) {
  TransportProbe *probe = context;
  assert(service == ROOT_RPC_SERVICE_ROOT);
  ++probe->calls;
  probe->last_command = command;
  const RuntimeRPCFieldHeader *empty = runtime_rpc_field_find(request, 0);
  assert(empty != NULL && runtime_rpc_field_payload_size(empty) == 0);
  const RuntimeRPCFieldHeader *span = runtime_rpc_field_find(request, 1);
  assert(span != NULL &&
         runtime_rpc_field_payload_size(span) == sizeof(RuntimeRPCPayloadSpan));

  if (command == ROOT_RPC_COMMAND_CONSOLE_WRITE) {
    const RuntimeRPCFieldHeader *channel = runtime_rpc_field_find(request, 2);
    const RuntimeRPCFieldHeader *flush = runtime_rpc_field_find(request, 3);
    assert(runtime_rpc_field_payload_size(channel) == sizeof(uint64_t));
    assert(runtime_rpc_field_payload_size(flush) == sizeof(uint8_t));
    probe->last_channel =
        *(const uint64_t *)runtime_rpc_field_payload_const(channel);
    probe->last_flush =
        *(const uint8_t *)runtime_rpc_field_payload_const(flush) != 0;
    const uint32_t result = UINT32_C(0x10203040);
    build_response(response_arena, response_capacity, &result, sizeof(result),
                   _Alignof(uint32_t), response_size);
    return 0;
  }
  assert(command == ROOT_RPC_COMMAND_MEMORY_READABLE);
  const RuntimeRPCFieldHeader *size = runtime_rpc_field_find(request, 2);
  assert(runtime_rpc_field_payload_size(size) == sizeof(uint64_t));
  probe->last_span_size =
      *(const uint64_t *)runtime_rpc_field_payload_const(size);
  const uint8_t result = probe->last_span_size <= 16 ? 1 : 0;
  build_response(response_arena, response_capacity, &result, sizeof(result),
                 _Alignof(uint8_t), response_size);
  return 0;
}

static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void invalid_client(void) {
  uint8_t byte = 0;
  RuntimeRPCPayloadSpan span = {
      .current = &byte,
      .lower_bound = &byte,
      .upper_bound = &byte + 1,
      .type_token = UINT64_C(0x100),
  };
  (void)root_rpc_console_write(NULL, span, 0, false);
}

static void test_commands(void) {
  uint8_t bytes[16] = {0};
  RuntimeRPCPayloadSpan span = {
      .current = bytes,
      .lower_bound = bytes,
      .upper_bound = bytes + sizeof(bytes),
      .type_token = UINT64_C(0x144f48),
  };
  TransportProbe probe = {0};
  RootRPCClient client = {.invoke = transport_invoke, .context = &probe};
  assert(root_rpc_console_write(&client, span, 9, true) ==
         UINT32_C(0x10203040));
  assert(probe.last_command == ROOT_RPC_COMMAND_CONSOLE_WRITE);
  assert(probe.last_channel == 9 && probe.last_flush);
  assert(root_rpc_memory_span_is_readable(&client, span));
  assert(probe.last_command == ROOT_RPC_COMMAND_MEMORY_READABLE);
  assert(probe.last_span_size == sizeof(bytes));
  assert(probe.calls == 2);
}

static void test_field_outliners(void) {
  uint64_t arena[64];
  RuntimeRPCBuilder builder;
  assert(root_rpc_outlined_initialize_sp28_builder_arguments(
             &builder, arena, sizeof(arena)) == &builder);
  assert(root_rpc_lifecycle_outlined_append_empty_field_zero(&builder) != NULL);
  uint64_t qword = 7;
  uint32_t word = 5;
  uint8_t byte = 1;
  RootRPCFieldArguments args = root_rpc_memory_outlined_stage_u64_field(&qword);
  assert(args.spec.size == 8 && args.spec.alignment == 8);
  args = root_rpc_console_outlined_prepare_u32_field_arguments(&word);
  assert(args.spec.size == 4 && args.spec.alignment == 4);
  args = root_rpc_memory_outlined_prepare_u64_field_arguments(&qword);
  assert(args.spec.size == 8);
  args = root_rpc_outlined_prepare_single_byte_field(&byte);
  assert(args.spec.size == 1);
  args = root_rpc_console_outlined_prepare_u64_field_arguments(&qword);
  assert(args.spec.size == 8);
  args = root_rpc_console_outlined_stage_u64_stack_field(&qword);
  assert(args.payload.current == &qword);
  args = root_rpc_console_outlined_prepare_byte_field_lengths(&byte);
  assert(args.spec.alignment == 1);
  args = root_rpc_console_outlined_stage_byte_stack_field(&byte);
  assert(args.payload.current == &byte);

  uint8_t payload_bytes[8];
  RuntimeRPCPayloadSpan source = {
      .current = payload_bytes,
      .lower_bound = payload_bytes,
      .upper_bound = payload_bytes + sizeof(payload_bytes),
      .type_token = UINT64_C(0x144f48),
  };
  assert(root_rpc_memory_outlined_save_source_span_x20_x23(source).current ==
         payload_bytes);
  assert(root_rpc_console_outlined_restore_response_span_x4_x7(source)
             .upper_bound == payload_bytes + sizeof(payload_bytes));
  args = root_rpc_console_outlined_save_builder_and_prepare_pointer_field(
      &source, source.type_token);
  assert(args.spec.size == 32 && args.spec.span);
  args = root_rpc_memory_outlined_prepare_32_byte_field_arguments(
      &source, source.type_token);
  assert(args.spec.size == sizeof(source));

  RootRPCBuilderBounds bounds =
      root_rpc_outlined_sp28_builder_span_x21_with_type(
          &builder, ROOT_RPC_BUILDER_TYPE_TOKEN);
  assert(bounds.builder == &builder && bounds.lower_bound == arena);
  bounds = root_rpc_outlined_sp28_builder_bounds_x21(&builder);
  assert(bounds.type_token == builder.arena_type_token);
  bounds = root_rpc_outlined_sp28_builder_span_x28_with_type(&builder, 0x88);
  assert(bounds.type_token == 0x88);
  assert(root_rpc_console_outlined_load_is_not_ptr_tag_x24() ==
         AVP_RUNTIME_RPC_NO_POINTER_TOKEN);
  assert(root_rpc_memory_outlined_load_ptr_is_raw_tag_x7() ==
         AVP_RUNTIME_RPC_RAW_POINTER_TOKEN);
  assert(root_rpc_console_outlined_load_builder_type_x23() ==
         ROOT_RPC_BUILDER_TYPE_TOKEN);
}

static void test_service_builder_and_frames(void) {
  uint64_t arena[64];
  RuntimeRPCBuilder builder;
  assert(root_rpc_console_outlined_initialize_sp20_builder_arguments(
             &builder, arena, sizeof(arena)) == &builder);
  uint32_t result = 3;
  RootRPCFieldArguments field =
      root_rpc_console_outlined_prepare_u32_field_arguments(&result);
  (void)runtime_rpc_builder_append_field(&builder, field.spec, field.payload);
  (void)runtime_rpc_builder_finalize(&builder);
  RuntimeRPCPacketView view = runtime_rpc_builder_view(&builder);
  RootRPCServiceCall call = root_rpc_outlined_prepare_service_call(view, 0x26);
  assert(call.service == ROOT_RPC_SERVICE_ROOT && call.command == 0x26);
  call = root_rpc_console_outlined_prepare_service_call(view, 0x22);
  assert(call.command == 0x22);
  RootRPCBuilderViewArguments prepared =
      root_rpc_console_outlined_prepare_builder_view_and_source_current(
          &builder, arena);
  assert(prepared.builder == &builder && prepared.source_current == arena);
  assert(root_rpc_memory_outlined_require_response_field(view, 0) != NULL);

  assert(root_rpc_outlined_stack_guard_load_to_x8() == __stack_chk_guard);
  assert(root_rpc_outlined_stack_guard_compare_saved_x8(__stack_chk_guard));
  uintptr_t registers[10];
  for (size_t index = 0; index < 10; ++index) {
    registers[index] = index + 10;
  }
  RootRPCRegisterFrame frame =
      root_rpc_outlined_prologue_save_x19_x28_allocate_c0(registers);
  (void)memset(registers, 0, sizeof(registers));
  root_rpc_outlined_epilogue_restore_x19_x28_release_c0(&frame, registers);
  for (size_t index = 0; index < 10; ++index) {
    assert(registers[index] == index + 10);
  }

  assert(root_rpc_console_outlined_clear_sp20_builder(&builder) == &builder);
  assert(builder.base == NULL);
  builder.base = arena;
  assert(root_rpc_console_outlined_clear_sp10_and_prepare_sp20_builder(
             &builder) == &builder);
  builder.base = arena;
  assert(root_rpc_lifecycle_outlined_clear_builder_frame(&builder) == &builder);
}

int main(void) {
  test_commands();
  test_field_outliners();
  test_service_builder_and_frames();
  expect_trap(invalid_client);
  return 0;
}

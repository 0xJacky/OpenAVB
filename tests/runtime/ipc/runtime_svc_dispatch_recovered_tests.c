#include "avp/runtime/ipc/runtime_svc_dispatch_recovered.h"
#include "avp/runtime/support/runtime_unprivileged_memory_recovered.h"
#include "avp/runtime/support/runtime_state.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


const RuntimeFirebloomType runtime_unprivileged_byte_type = {
    .identity = UINT64_C(0x62797465),
    .element_size = 1,
    .bytewise_copyable = true,
};

static RuntimePageTableStatus captured_map_status;
static uintptr_t captured_virtual;
static uintptr_t captured_physical;
static size_t captured_size;
static uint8_t captured_regime;
static uint8_t captured_attribute;
static uint32_t captured_flags;
static uintptr_t captured_owner_lower;
static uintptr_t captured_owner_upper;
static uint64_t captured_owner_type;
static RuntimeFirebloomSpan resolved_service_span;
static uint16_t published_handle = 0x35;
static uint8_t captured_capability_type;
static uint8_t captured_capability_rights;
static uint16_t captured_capability_audience;
static RuntimeGlobalState runtime_global_state;
static RuntimeObjectRecord object_records[4];
static uint32_t capability_entries[4];

uint64_t runtime_address_space_source_token(void) {
  return UINT64_C(0x01e87eca99c7dc29);
}

volatile RuntimeGlobalState *runtime_state_get(void) {
  return &runtime_global_state;
}

RuntimeFirebloomSpan runtime_object_registry_span(void) {
  return (RuntimeFirebloomSpan){object_records, object_records,
                                object_records + 4, NULL};
}

RuntimeFirebloomSpan runtime_capability_registry_span(void) {
  return (RuntimeFirebloomSpan){capability_entries, capability_entries,
                                capability_entries + 4, NULL};
}

void runtime_copy_from_unprivileged(RuntimeFirebloomSpan destination,
                                    const void *source, size_t size) {
  assert(destination.current != NULL && source != NULL);
  uint8_t *output = destination.current;
  const uint8_t *input = source;
  for (size_t index = 0; index < size; ++index) {
    output[index] = input[index];
  }
}

void runtime_bzero(void *destination, size_t size) {
  uint8_t *bytes = destination;
  for (size_t index = 0; index < size; ++index) {
    bytes[index] = 0;
  }
}

void *runtime_firebloom_copy_span(RuntimeFirebloomSpan destination,
                                  RuntimeFirebloomSpan source, size_t size) {
  assert(destination.current != NULL && source.current != NULL);
  assert((uintptr_t)destination.current >= (uintptr_t)destination.lower_bound);
  assert((uintptr_t)destination.upper_bound -
             (uintptr_t)destination.current >=
         size);
  uint8_t *output = destination.current;
  const uint8_t *input = source.current;
  for (size_t index = 0; index < size; ++index) {
    output[index] = input[index];
  }
  return destination.current;
}

RuntimePageTableStatus runtime_page_table_update_range(
    uintptr_t virtual_address, uintptr_t physical_address, size_t size,
    uint8_t regime, uint8_t memory_attribute, uint32_t flags,
    uintptr_t owner_lower, uintptr_t owner_upper, uint64_t owner_type) {
  captured_virtual = virtual_address;
  captured_physical = physical_address;
  captured_size = size;
  captured_regime = regime;
  captured_attribute = memory_attribute;
  captured_flags = flags;
  captured_owner_lower = owner_lower;
  captured_owner_upper = owner_upper;
  captured_owner_type = owner_type;
  return captured_map_status;
}

uint16_t runtime_capability_audience_for_domain(uint8_t domain) {
  assert(domain > 0 && domain <= 15);
  return (uint16_t)(UINT16_C(1) << domain);
}

uint16_t runtime_capability_publish(void *object,
                                    const RuntimeFirebloomType *type,
                                    uint8_t capability_type, uint8_t rights,
                                    uint16_t audience) {
  assert(object != NULL && type != NULL);
  captured_capability_type = capability_type;
  captured_capability_rights = rights;
  captured_capability_audience = audience;
  return published_handle;
}

RuntimeFirebloomSpan runtime_capability_resolve_span(
    uint16_t handle, uint8_t type, uint8_t required_rights,
    size_t minimum_size) {
  assert(handle == 0x77);
  assert(type == RUNTIME_CAPABILITY_TYPE_OBJECT);
  assert(required_rights == RUNTIME_CAPABILITY_RIGHT_WAIT);
  assert(minimum_size == 0x90);
  return resolved_service_span;
}

typedef struct {
  uint32_t handler_calls;
  RuntimeSVCRequest last_request;
  uint32_t callback0_calls;
  uint32_t callback1_calls;
  uint32_t callback2_calls;
  uint32_t hypervisor_calls;
} SVCOracle;

static RuntimeSVCResponse svc_handler(void *context,
                                      const RuntimeSVCRequest *request) {
  SVCOracle *oracle = context;
  ++oracle->handler_calls;
  oracle->last_request = *request;
  return (RuntimeSVCResponse){
      .x = {request->x[0] + request->x[6], request->svc_number, 0, 0},
  };
}

static uint64_t callback0(uint64_t argument0, uint64_t argument1,
                          const RuntimeFirebloomType *argument1_type,
                          uint64_t context_current,
                          uint64_t context_lower_bound,
                          uint64_t context_upper_bound,
                          const RuntimeFirebloomType *context_type) {
  (void)argument1_type;
  (void)context_type;
  SVCOracle *oracle = runtime_svc_dispatch_recovered_state()->context;
  ++oracle->callback0_calls;
  return argument0 + argument1 + context_current + context_lower_bound +
         context_upper_bound;
}

static uint64_t callback1(uint64_t argument) {
  SVCOracle *oracle = runtime_svc_dispatch_recovered_state()->context;
  ++oracle->callback1_calls;
  return argument + 1;
}

static uint64_t callback2(uint64_t argument) {
  SVCOracle *oracle = runtime_svc_dispatch_recovered_state()->context;
  ++oracle->callback2_calls;
  return argument + 2;
}

static uint64_t hypervisor_call(void *context, uint64_t argument0,
                                uint64_t argument1, uint64_t argument2,
                                uint64_t argument3) {
  SVCOracle *oracle = context;
  ++oracle->hypervisor_calls;
  return argument0 ^ argument1 ^ argument2 ^ argument3;
}

static void initialize(RuntimeSVCDispatchRecoveredState *state,
                       SVCOracle *oracle) {
  runtime_svc_dispatch_recovered_initialize(state, oracle);
  runtime_global_state = (RuntimeGlobalState){0};
  state->callbacks = (RuntimeSVCCallbackTable){
      .slot0 = callback0,
      .slot1 = callback1,
      .slot2 = callback2,
  };
  state->hypervisor_call = hypervisor_call;
  runtime_svc_dispatch_recovered_install(state);
}

static void test_callbacks_and_dispatch(void) {
  RuntimeSVCDispatchRecoveredState state;
  SVCOracle oracle = {0};
  initialize(&state, &oracle);
  assert(runtime_svc_callback_slot0_default().current == NULL);
  assert(runtime_svc_outlined_return_null_span().current == NULL);
  assert(runtime_svc_callback_slot1_default_noop(8) == 8);
  assert(runtime_svc_callback_slot2_default_noop(9) == 9);
  RuntimeFirebloomSpan table = runtime_svc_callback_table_span();
  assert(table.current == &state.callbacks);
  assert((uintptr_t)table.upper_bound - (uintptr_t)table.current ==
         sizeof(state.callbacks));
  assert(runtime_svc_callback_slot0_dispatch(1, 2, NULL, 3, 4, 5, NULL) ==
         15);
  assert(runtime_svc_callback_slot1_dispatch(10) == 11);
  assert(runtime_svc_callback_slot2_dispatch(10) == 12);

  state.handlers[0x42] = svc_handler;
  runtime_svc_dispatch_recovered_allow(&state, 0x42);
  runtime_svc_dispatch(1, 2, 3, 4, 5, 6, 7, 0x42);
  assert(state.last_response.x[0] == 8 &&
         state.last_response.x[1] == 0x42);
  assert(oracle.handler_calls == 1);
  assert(oracle.last_request.x[5] == 6 && oracle.last_request.x[6] == 7);
  assert(runtime_hypervisor_call_zero(1, 2, 4, 8) == 15);
  assert(oracle.hypervisor_calls == 1);
}

static void test_copy_and_page_helpers(void) {
  RuntimeSVCDispatchRecoveredState state;
  SVCOracle oracle = {0};
  initialize(&state, &oracle);
  const uint8_t source[] = "diagnostic";
  uint8_t output[5] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
  runtime_copy_unprivileged_c_string_bounded(
      output, (uintptr_t)output, (uintptr_t)(output + sizeof(output)), source,
      sizeof(output));
  assert(output[0] == 'd' && output[3] == 'g' && output[4] == 0);

  static _Alignas(RUNTIME_PAGE_TABLE_PAGE_SIZE)
      uint8_t page[RUNTIME_PAGE_TABLE_PAGE_SIZE];
  for (size_t index = 0; index < sizeof(page); ++index) {
    page[index] = 0x5a;
  }
  runtime_svc_outlined_zero_16k_page(page);
  for (size_t index = 0; index < sizeof(page); ++index) {
    assert(page[index] == 0);
  }

  uintptr_t owner = UINT64_C(0x1234);
  captured_map_status = RUNTIME_PAGE_TABLE_STATUS_OK;
  assert(runtime_svc_outlined_map_identity_attribute0_flags11(
             0x4000, 0x8000, 0x4000, 2, (void *)owner,
             (void *)(uintptr_t)UINT64_C(0x1000)) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(captured_virtual == 0x4000 && captured_physical == 0x8000);
  assert(captured_size == 0x4000 && captured_regime == 2);
  assert(captured_attribute == 0 && captured_flags == 0x11);
  assert(captured_owner_lower == 0x1000);
  assert(captured_owner_upper == 0x1000 + sizeof(uintptr_t));
  assert(captured_owner_type == owner);

  uint8_t copy_source[] = {1, 2, 3, 4};
  uint8_t copy_destination[4] = {0};
  RuntimeFirebloomType bytes = {
      .identity = 1, .element_size = 1, .bytewise_copyable = true};
  RuntimeFirebloomSpan copied = runtime_svc_outlined_copy_span_with_saved_source(
      (RuntimeFirebloomSpan){copy_destination, copy_destination,
                             copy_destination + sizeof(copy_destination),
                             &bytes},
      sizeof(copy_source), copy_source, &bytes);
  assert(copied.current == copy_destination && copy_destination[3] == 4);
}

static void test_service_and_rpc_helpers(void) {
  RuntimeSVCDispatchRecoveredState state;
  SVCOracle oracle = {0};
  initialize(&state, &oracle);
  uint64_t service_storage[0x12] = {0};
  uint16_t handle = runtime_service_capability_publish(
      service_storage, service_storage, service_storage + 0x12,
      &state.service_type, 3, 5);
  assert(handle == published_handle);
  assert(captured_capability_type == RUNTIME_CAPABILITY_TYPE_OBJECT);
  assert(captured_capability_rights == 5);
  assert(captured_capability_audience == 8);

  published_handle = 0x36;
  assert(runtime_service_capability_publish(
             service_storage, service_storage, service_storage + 0x12,
             &state.service_type, 3, 0) == 0x36);
  assert(captured_capability_rights == 0);
  published_handle = 0x35;

  runtime_global_state.lifecycle_flags = 1;
  runtime_global_state.object_count = 2;
  runtime_global_state.capability_count = 2;
  object_records[1] = (RuntimeObjectRecord){service_storage,
                                            &state.service_type};
  capability_entries[1] =
      ((uint32_t)RUNTIME_CAPABILITY_TYPE_OBJECT << 28) |
      ((uint32_t)5 << 25) | ((uint32_t)4 << 10) | 1;
  assert(runtime_service_capability_publish(
             service_storage, service_storage, service_storage + 0x12,
             &state.service_type, 3, 5) == 1);
  runtime_global_state.lifecycle_flags = 0;

  uint64_t elements[4] = {0};
  uint64_t count = 4;
  RuntimeFirebloomSpan count_span =
      runtime_service_require_counted_u64_vector(elements, &count);
  assert(count_span.current == &count);
  assert(count_span.upper_bound == &count + 1);

  resolved_service_span = (RuntimeFirebloomSpan){
      service_storage, service_storage, service_storage + 0x12,
      &state.service_type};
  RuntimeFirebloomSpan resolved =
      runtime_service_resolve_rights1_capability_from_x23(0x77);
  assert(resolved.current == service_storage);

  RuntimeFirebloomTriplet triplet =
      runtime_rpc_outlined_restore_builder_tuple_x22_x24_x27(
          elements, elements, elements + 4);
  assert(triplet.current == elements && triplet.upper_bound == elements + 4);
}

static void expect_trap(void (*operation)(void)) {
  pid_t child = fork();
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

static RuntimeSVCDispatchRecoveredState fatal_state;
static SVCOracle fatal_oracle;

static void fatal_bad_svc(void) {
  runtime_svc_dispatch(0, 0, 0, 0, 0, 0, 0, 0x44);
}

static void fatal_bad_count(void) {
  uint64_t elements[1] = {0};
  uint64_t count = 0;
  (void)runtime_service_require_counted_u64_vector(elements, &count);
}

static void fatal_bad_copy_bounds(void) {
  uint8_t destination[4] = {0};
  const uint8_t source[] = "x";
  runtime_copy_unprivileged_c_string_bounded(
      destination, (uintptr_t)(destination + 1),
      (uintptr_t)(destination + sizeof(destination)), source,
      sizeof(destination));
}

static void fatal_bad_service_span(void) {
  uint64_t storage[2] = {0};
  (void)runtime_service_capability_publish(
      storage, storage, storage + 2, &fatal_state.service_type, 1, 1);
}

static void test_fatal_contracts(void) {
  initialize(&fatal_state, &fatal_oracle);
  expect_trap(fatal_bad_svc);
  initialize(&fatal_state, &fatal_oracle);
  expect_trap(fatal_bad_count);
  initialize(&fatal_state, &fatal_oracle);
  expect_trap(fatal_bad_copy_bounds);
  initialize(&fatal_state, &fatal_oracle);
  expect_trap(fatal_bad_service_span);
  initialize(&fatal_state, &fatal_oracle);
  expect_trap(runtime_svc_bootstrap_memory_command_fatal);
}

int main(void) {
  test_callbacks_and_dispatch();
  test_copy_and_page_helpers();
  test_service_and_rpc_helpers();
  test_fatal_contracts();
  puts("runtime_svc_dispatch_recovered_tests: ok");
  return 0;
}

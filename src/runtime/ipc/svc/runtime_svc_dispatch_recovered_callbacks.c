#include "runtime_svc_dispatch_recovered_private.h"


#if defined(__ELF__)
static RuntimeSVCDispatchRecoveredState *runtime_svc_active_state
    __attribute__((section(".runtime_task.state")));
#else
static RuntimeSVCDispatchRecoveredState *runtime_svc_active_state;
#endif

_Noreturn void runtime_svc_dispatch_recovered_trap(void) {
  __builtin_trap();
}

RuntimeSVCDispatchRecoveredState *runtime_svc_dispatch_recovered_state(void) {
  if (runtime_svc_active_state == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  return runtime_svc_active_state;
}

bool runtime_svc_dispatch_recovered_contains(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    size_t size
) {
  const uintptr_t value = (uintptr_t)current;
  const uintptr_t lower = (uintptr_t)lower_bound;
  const uintptr_t upper = (uintptr_t)upper_bound;
  return current != 0 && lower <= value && value < upper &&
         size <= upper - value;
}

static uint64_t runtime_svc_default_slot0_value(
    uint64_t argument0,
    uint64_t argument1,
    const RuntimeFirebloomType *argument1_type,
    uint64_t context_current,
    uint64_t context_lower_bound,
    uint64_t context_upper_bound,
    const RuntimeFirebloomType *context_type
) {
  (void)argument0;
  (void)argument1;
  (void)argument1_type;
  (void)context_current;
  (void)context_lower_bound;
  (void)context_upper_bound;
  (void)context_type;
  return 0;
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_slot0_default(void) {
  return (RuntimeFirebloomSpan){0};
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_slot1_default_noop(uint64_t argument) {
  return argument;
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_slot2_default_noop(uint64_t argument) {
  return argument;
}

void runtime_svc_dispatch_recovered_initialize(
    RuntimeSVCDispatchRecoveredState *state,
    void *context
) {
  if (state == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  uint64_t *words = (uint64_t *)state;
  for (size_t index = 0; index < sizeof(*state) / sizeof(*words); ++index) {
    words[index] = 0;
  }
  state->context = context;
  state->callbacks.slot0 = runtime_svc_default_slot0_value;
  state->callbacks.slot0_signature = 0;
  state->callbacks.slot1 = runtime_svc_callback_slot1_default_noop;
  state->callbacks.slot1_signature = 0;
  state->callbacks.slot2 = runtime_svc_callback_slot2_default_noop;
  state->callbacks.slot2_signature = 0;
  state->service_type = (RuntimeFirebloomType){
      .identity = UINT64_C(0x7365727669636536),
      .element_size = 0x90,
      .bytewise_copyable = false,
  };
  state->target_domain = 1;
  state->service_rights_policy = 1;
}

void runtime_svc_dispatch_recovered_install(
    RuntimeSVCDispatchRecoveredState *state) {
  if (state == 0 || state->callbacks.slot0 == 0 ||
      state->callbacks.slot1 == 0 || state->callbacks.slot2 == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  runtime_svc_active_state = state;
}

void runtime_svc_dispatch_recovered_allow(
    RuntimeSVCDispatchRecoveredState *state,
    uint16_t svc_number
) {
  if (state == 0 || svc_number > 0x100) {
    runtime_svc_dispatch_recovered_trap();
  }
  state->permission_bitmap[svc_number >> 3] |=
      (uint8_t)(UINT8_C(1) << (svc_number & 7));
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_table_span(void) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  return (RuntimeFirebloomSpan){
      .current = &state->callbacks,
      .lower_bound = &state->callbacks,
      .upper_bound = &state->callbacks + 1,
      .type = 0,
  };
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_outlined_return_null_span(void) {
  return (RuntimeFirebloomSpan){0};
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC runtime_svc_callback_slot0_dispatch(
    uint64_t argument0,
    uint64_t argument1,
    const RuntimeFirebloomType *argument1_type,
    uint64_t context_current,
    uint64_t context_lower_bound,
    uint64_t context_upper_bound,
    const RuntimeFirebloomType *context_type
) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->callbacks.slot0 == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  return state->callbacks.slot0(
      argument0,
      argument1,
      argument1_type,
      context_current,
      context_lower_bound,
      context_upper_bound,
      context_type
  );
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_slot1_dispatch(uint64_t argument) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->callbacks.slot1 == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  return state->callbacks.slot1(argument);
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_callback_slot2_dispatch(uint64_t argument) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->callbacks.slot2 == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  return state->callbacks.slot2(argument);
}

RuntimeFirebloomTriplet RUNTIME_SVC_DISPATCH_PUBLIC
runtime_rpc_outlined_restore_builder_tuple_x22_x24_x27(
    void *current,
    const void *lower_bound,
    const void *upper_bound
) {
  return (RuntimeFirebloomTriplet){
      .current = current,
      .lower_bound = (void *)(uintptr_t)lower_bound,
      .upper_bound = (void *)(uintptr_t)upper_bound,
  };
}

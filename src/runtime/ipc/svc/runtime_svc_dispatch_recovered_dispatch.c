#include "runtime_svc_dispatch_recovered_private.h"

#include "avp/runtime/support/runtime_state.h"


static const RuntimeFirebloomType runtime_service_vector_u64_type = {
    .identity = UINT64_C(0x00144f30),
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = true,
};

static uint16_t runtime_service_find_prepublished_capability(
    void *service_current,
    const RuntimeFirebloomType *service_type,
    uint8_t target_domain,
    uint8_t rights_policy
) {
  const volatile RuntimeGlobalState *global = runtime_state_get();
  RuntimeFirebloomSpan objects = runtime_object_registry_span();
  RuntimeObjectRecord *object_records = objects.current;
  uint16_t object_id = 0;
  for (uint16_t candidate = 1; candidate < global->object_count; ++candidate) {
    RuntimeObjectRecord *record = &object_records[candidate];
    if (record->object == service_current && record->type != 0 &&
        record->type->identity == service_type->identity &&
        record->type->element_size >= 0x90) {
      object_id = candidate;
      break;
    }
  }
  if (object_id == 0) {
    runtime_svc_dispatch_recovered_trap();
  }

  RuntimeFirebloomSpan capabilities = runtime_capability_registry_span();
  const uint32_t *entries = capabilities.current;
  const uint16_t audience_bit = (uint16_t)(UINT16_C(1) << (target_domain - 1));
  for (uint16_t handle = 1; handle < global->capability_count; ++handle) {
    const uint32_t entry = entries[handle];
    const uint8_t type = (uint8_t)(entry >> 28);
    const uint8_t rights = (uint8_t)((entry >> 25) & 7);
    const uint16_t audience = (uint16_t)((entry >> 10) & 0x7fff);
    if ((entry & 0x3ff) == object_id &&
        type == RUNTIME_CAPABILITY_TYPE_OBJECT &&
        (rights & rights_policy) == rights_policy &&
        (audience & audience_bit) != 0) {
      return handle;
    }
  }
  runtime_svc_dispatch_recovered_trap();
}

#if !defined(__ELF__)
uint64_t RUNTIME_SVC_DISPATCH_PUBLIC runtime_hypervisor_call_zero(
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3
) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->hypervisor_call == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  return state->hypervisor_call(
      state->context,
      argument0,
      argument1,
      argument2,
      argument3
  );
}
#endif

void RUNTIME_SVC_DISPATCH_PUBLIC runtime_svc_dispatch(
    uint64_t x0,
    uint64_t x1,
    uint64_t x2,
    uint64_t x3,
    uint64_t x4,
    uint64_t x5,
    uint64_t x6,
    uint16_t svc_number
) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (svc_number > 0x100 ||
      (state->permission_bitmap[svc_number >> 3] &
       (UINT8_C(1) << (svc_number & 7))) == 0 ||
      state->handlers[svc_number] == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  const RuntimeSVCRequest request = {
      .x = {x0, x1, x2, x3, x4, x5, x6},
      .svc_number = svc_number,
  };
  state->last_response = state->handlers[svc_number](state->context, &request);
}

uint16_t RUNTIME_SVC_DISPATCH_PUBLIC runtime_service_capability_publish(
    void *service_current,
    const void *service_lower_bound,
    const void *service_upper_bound,
    const RuntimeFirebloomType *service_type,
    uint8_t target_domain,
    uint8_t rights_policy
) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (service_type == 0 || service_type->element_size != 0x90 ||
      service_type->identity != state->service_type.identity ||
      !runtime_svc_dispatch_recovered_contains(service_current,
                                               service_lower_bound,
                                               service_upper_bound, 0x90) ||
      target_domain == 0 || target_domain > 15 ||
      (rights_policy & ~UINT8_C(7)) != 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  const uint16_t audience = runtime_capability_audience_for_domain(target_domain);
  const volatile RuntimeGlobalState *global = runtime_state_get();
  if ((global->lifecycle_flags & UINT8_C(1)) != 0) {
    /* Bootstrap sealing freezes registry mutation, so late callers recover the
       equivalent capability published during bootstrap. */
    return runtime_service_find_prepublished_capability(
        service_current,
        service_type,
        target_domain,
        rights_policy
    );
  }
  return runtime_capability_publish(
      service_current,
      service_type,
      RUNTIME_CAPABILITY_TYPE_OBJECT,
      rights_policy,
      audience
  );
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_service_require_counted_u64_vector(
    uint64_t *elements,
    const uint64_t *count_slot
) {
  if (elements == 0 || count_slot == 0 || *count_slot == 0 ||
      *count_slot > 0x3ff ||
      *count_slot > SIZE_MAX / sizeof(*elements)) {
    runtime_svc_dispatch_recovered_trap();
  }
  return (RuntimeFirebloomSpan){
      .current = (void *)(uintptr_t)count_slot,
      .lower_bound = (void *)(uintptr_t)count_slot,
      .upper_bound = (void *)(uintptr_t)(count_slot + 1),
      .type = &runtime_service_vector_u64_type,
  };
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_service_resolve_rights1_capability_from_x23(uint16_t handle) {
  return runtime_capability_resolve_span(
      handle,
      RUNTIME_CAPABILITY_TYPE_OBJECT,
      RUNTIME_CAPABILITY_RIGHT_WAIT,
      0x90
  );
}

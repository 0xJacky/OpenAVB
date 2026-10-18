#pragma once

#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/memory/runtime_page_table_recovered.h"
#include "avp/runtime/ipc/runtime_rpc.h"
#include "avp/runtime/ipc/runtime_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_SVC_DISPATCH_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_SVC_DISPATCH_PUBLIC __attribute__((used))
#endif

typedef uint64_t (*RuntimeSVCCallbackSlot0)(
    uint64_t argument0,
    uint64_t argument1,
    const RuntimeFirebloomType *argument1_type,
    uint64_t context_current,
    uint64_t context_lower_bound,
    uint64_t context_upper_bound,
    const RuntimeFirebloomType *context_type
);
typedef uint64_t (*RuntimeSVCCallbackSlotN)(uint64_t argument);

typedef struct {
  RuntimeSVCCallbackSlot0 slot0;
  const RuntimeFirebloomType *slot0_signature;
  RuntimeSVCCallbackSlotN slot1;
  const RuntimeFirebloomType *slot1_signature;
  RuntimeSVCCallbackSlotN slot2;
  const RuntimeFirebloomType *slot2_signature;
} RuntimeSVCCallbackTable;

_Static_assert(sizeof(RuntimeSVCCallbackTable) == 0x30,
               "the recovered table carries three typed callback bindings");
_Static_assert(offsetof(RuntimeSVCCallbackTable, slot1) == 0x10,
               "slot one retains its recovered table offset");
_Static_assert(offsetof(RuntimeSVCCallbackTable, slot2) == 0x20,
               "slot two retains its recovered table offset");

typedef struct {
  void *current;
  void *lower_bound;
  void *upper_bound;
} RuntimeFirebloomTriplet;

typedef struct {
  uint64_t x[7];
  uint16_t svc_number;
} RuntimeSVCRequest;

typedef struct {
  uint64_t x[4];
  RuntimeFirebloomSpan span;
  bool has_span;
} RuntimeSVCResponse;

typedef RuntimeSVCResponse (*RuntimeSVCRecoveredHandler)(
    void *context,
    const RuntimeSVCRequest *request
);
typedef uint64_t (*RuntimeSVCHypervisorCall)(
    void *context,
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3
);
typedef void (*RuntimeSVCFatalHandler)(
    void *context,
    uint64_t source_token,
    uint32_t source_line,
    const char *short_diagnostic,
    const char *long_diagnostic
);

typedef struct {
  void *context;
  RuntimeSVCRecoveredHandler handlers[0x101];
  uint8_t permission_bitmap[0x21];
  RuntimeSVCHypervisorCall hypervisor_call;
  RuntimeSVCFatalHandler fatal;
  RuntimePageTable *page_table;
  RuntimeSVCCallbackTable callbacks;
  RuntimeFirebloomType service_type;
  uint8_t target_domain;
  uint8_t service_rights_policy;
  RuntimeSVCResponse last_response;
} RuntimeSVCDispatchRecoveredState;

void runtime_svc_dispatch_recovered_initialize(
    RuntimeSVCDispatchRecoveredState *state,
    void *context
);
void runtime_svc_dispatch_recovered_install(
    RuntimeSVCDispatchRecoveredState *state);
RuntimeSVCDispatchRecoveredState *runtime_svc_dispatch_recovered_state(void);
void runtime_svc_dispatch_recovered_allow(
    RuntimeSVCDispatchRecoveredState *state,
    uint16_t svc_number
);

RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_svc_callback_slot0_default(void);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t
runtime_svc_callback_slot1_default_noop(uint64_t argument);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t
runtime_svc_callback_slot2_default_noop(uint64_t argument);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_svc_callback_table_span(void);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_svc_outlined_return_null_span(void);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t runtime_svc_callback_slot0_dispatch(
    uint64_t argument0,
    uint64_t argument1,
    const RuntimeFirebloomType *argument1_type,
    uint64_t context_current,
    uint64_t context_lower_bound,
    uint64_t context_upper_bound,
    const RuntimeFirebloomType *context_type
);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t
runtime_svc_callback_slot1_dispatch(uint64_t argument);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t
runtime_svc_callback_slot2_dispatch(uint64_t argument);

RUNTIME_SVC_DISPATCH_PUBLIC void runtime_copy_unprivileged_c_string_bounded(
    uint8_t *destination,
    uintptr_t destination_lower_bound,
    uintptr_t destination_upper_bound,
    const uint8_t *source,
    uint64_t capacity
);
RUNTIME_SVC_DISPATCH_PUBLIC _Noreturn void
runtime_svc_copy_unprivileged_diagnostic_and_fatal(
    const uint8_t *short_diagnostic,
    const uint8_t *long_diagnostic
);
RUNTIME_SVC_DISPATCH_PUBLIC _Noreturn void
runtime_svc_bootstrap_memory_command_fatal(void);
RUNTIME_SVC_DISPATCH_PUBLIC void
runtime_svc_outlined_zero_16k_page(void *page);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t
runtime_svc_outlined_map_identity_attribute0_flags11(
    uintptr_t virtual_base,
    uintptr_t physical_base,
    size_t byte_size,
    uint32_t translation_regime,
    void *owner,
    void *owner_lower_bound
);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_svc_outlined_copy_span_with_saved_source(
    RuntimeFirebloomSpan destination,
    size_t byte_count,
    const void *source_current,
    const RuntimeFirebloomType *source_type
);

RUNTIME_SVC_DISPATCH_PUBLIC void runtime_svc_dispatch(
    uint64_t x0,
    uint64_t x1,
    uint64_t x2,
    uint64_t x3,
    uint64_t x4,
    uint64_t x5,
    uint64_t x6,
    uint16_t svc_number
);
RUNTIME_SVC_DISPATCH_PUBLIC uint64_t runtime_hypervisor_call_zero(
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3
);

RUNTIME_SVC_DISPATCH_PUBLIC uint16_t runtime_service_capability_publish(
    void *service_current,
    const void *service_lower_bound,
    const void *service_upper_bound,
    const RuntimeFirebloomType *service_type,
    uint8_t target_domain,
    uint8_t rights_policy
);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_service_require_counted_u64_vector(
    uint64_t *elements,
    const uint64_t *count_slot
);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomSpan
runtime_service_resolve_rights1_capability_from_x23(uint16_t handle);
RUNTIME_SVC_DISPATCH_PUBLIC RuntimeFirebloomTriplet
runtime_rpc_outlined_restore_builder_tuple_x22_x24_x27(
    void *current,
    const void *lower_bound,
    const void *upper_bound
);

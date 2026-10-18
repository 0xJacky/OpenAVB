#pragma once

#include "avp/runtime/ipc/runtime_domain.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_DOMAIN_RECOVERED_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_DOMAIN_RECOVERED_PUBLIC __attribute__((used))
#endif

typedef struct {
  uint8_t *current;
  uint8_t *lower_bound;
  uint8_t *upper_bound;
} RuntimeDomainByteView;

typedef struct {
  uint16_t *current;
  uint16_t *lower_bound;
  uint16_t *upper_bound;
} RuntimeDomainHandleView;

typedef struct {
  uint32_t *current;
  uint32_t *lower_bound;
  uint32_t *upper_bound;
} RuntimeDomainWordView;

typedef struct {
  uint64_t *current;
  uint64_t *lower_bound;
  uint64_t *upper_bound;
} RuntimeDomainIdentityView;

typedef struct {
  RuntimeDomain *current;
  RuntimeDomain *lower_bound;
  RuntimeDomain *upper_bound;
} RuntimeDomainView;

typedef struct {
  uintptr_t primary_virtual_base;
  size_t primary_size;
  uint64_t primary_reserved;
  uint8_t reserved_18[0x20];
} RuntimeCurrentDomainMetadata;

typedef struct {
  uint64_t reserved_00;
  uint16_t capability_handle;
  uint8_t reserved_0a[6];
} RuntimeDomainCapabilityRecord;

_Static_assert(sizeof(RuntimeCurrentDomainMetadata) == 0x38,
               "current domain metadata size");
_Static_assert(offsetof(RuntimeDomainCapabilityRecord, capability_handle) ==
                   0x08,
               "domain capability record handle offset");

typedef RuntimeDomainView (*RuntimeDomainRecoveredObjectResolve)(
  uint64_t object_id,
  size_t object_size,
  void *context
);
typedef RuntimeDomainView (*RuntimeDomainRecoveredCapabilityResolve)(
  uint16_t handle,
  uint8_t required_rights,
  void *context
);
typedef RuntimeDomain *(*RuntimeDomainRecoveredLookup)(
  uint8_t domain_id,
  void *context
);

typedef struct {
  uint8_t *current_index;
  uint16_t *current_handle;
  uint64_t *current_identity;
  RuntimeCurrentDomainMetadata *current_metadata;
  const uint8_t *selector_bytes;
  size_t selector_nibble_count;
  RuntimeDomainRecoveredObjectResolve resolve_object;
  RuntimeDomainRecoveredCapabilityResolve resolve_capability;
  RuntimeDomainRecoveredLookup lookup;
  void *context;
} RuntimeDomainRecoveredConfiguration;

void runtime_domain_recovered_configure(
    const RuntimeDomainRecoveredConfiguration *configuration);

RUNTIME_DOMAIN_RECOVERED_PUBLIC bool
runtime_current_domain_index_equals(uint32_t expected_index);
RUNTIME_DOMAIN_RECOVERED_PUBLIC RuntimeDomainByteView
runtime_current_domain_index_bounded_view(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC uint8_t runtime_current_domain_index(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC RuntimeDomainView
runtime_domain_resolve_object_id(uint64_t object_id);
RUNTIME_DOMAIN_RECOVERED_PUBLIC bool
runtime_domain_registry_contains_id(uint64_t domain_id);
RUNTIME_DOMAIN_RECOVERED_PUBLIC RuntimeDomainIdentityView
runtime_current_domain_identity_bounded_view(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC RuntimeDomainHandleView
runtime_current_domain_handle_bounded_view(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC uint64_t *
runtime_domain_identity_tuple_validate(RuntimeDomainIdentityView identity);
RUNTIME_DOMAIN_RECOVERED_PUBLIC void
runtime_current_domain_metadata_publish(RuntimeDomainView domain);
RUNTIME_DOMAIN_RECOVERED_PUBLIC _Noreturn void
runtime_domain_record_bounds_panic(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC uint8_t
runtime_domain_selector_lookup_id(uint32_t selector_position);
RUNTIME_DOMAIN_RECOVERED_PUBLIC void
runtime_domain_selector_decode_nibble_position(
  uint32_t selector_position,
  RuntimeDomainWordView byte_index,
  RuntimeDomainWordView shift
);
RUNTIME_DOMAIN_RECOVERED_PUBLIC uint16_t
runtime_domain_public_handle_for_id(uint32_t domain_id);
RUNTIME_DOMAIN_RECOVERED_PUBLIC uint16_t runtime_current_domain_handle(void);
RUNTIME_DOMAIN_RECOVERED_PUBLIC RuntimeDomainView
runtime_domain_resolve_wait_signal_capability_from_x10_record(
    const RuntimeDomainCapabilityRecord *record);

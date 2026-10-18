#include "avp/runtime/ipc/runtime_domain_recovered.h"

#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/support/runtime_state.h"

static uint64_t recovered_current_identity;
static RuntimeCurrentDomainMetadata recovered_current_metadata;
static RuntimeDomainRecoveredConfiguration domain_configuration;
static bool domain_configuration_bound;

static const RuntimeFirebloomType runtime_domain_metadata_byte_type = {
    .identity = UINT64_C(0x646f6d6d657461),
    .element_size = 1,
    .bytewise_copyable = true,
};

static _Noreturn void runtime_domain_recovered_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static bool runtime_domain_address_in_bounds(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
  return current != 0 && lower_bound != 0 && upper_bound != 0 &&
         current >= lower_bound && current < upper_bound &&
         size <= upper_bound - current;
}

static bool runtime_domain_view_is_valid(const RuntimeDomainView view) {
  return runtime_domain_address_in_bounds(
      (uintptr_t)view.current,
      (uintptr_t)view.lower_bound,
      (uintptr_t)view.upper_bound,
      sizeof(RuntimeDomain)
  );
}

static uint8_t *runtime_domain_current_index_pointer(void) {
  if (domain_configuration_bound &&
      domain_configuration.current_index != NULL) {
    return domain_configuration.current_index;
  }
  return &runtime_execution_domain;
}

static uint16_t *runtime_domain_current_handle_pointer(void) {
  if (domain_configuration_bound &&
      domain_configuration.current_handle != NULL) {
    return domain_configuration.current_handle;
  }
  volatile RuntimeGlobalState *state = runtime_state_get();
  return (uint16_t *)(uintptr_t)&state->runtime_handle;
}

static uint64_t *runtime_domain_current_identity_pointer(void) {
  if (domain_configuration_bound &&
      domain_configuration.current_identity != NULL) {
    return domain_configuration.current_identity;
  }
  return &recovered_current_identity;
}

static RuntimeCurrentDomainMetadata *
runtime_domain_current_metadata_pointer(void) {
  if (domain_configuration_bound &&
      domain_configuration.current_metadata != NULL) {
    return domain_configuration.current_metadata;
  }
  return &recovered_current_metadata;
}

static RuntimeDomain *runtime_domain_lookup_operation(uint8_t domain_id) {
  if (domain_configuration_bound && domain_configuration.lookup != NULL) {
    return domain_configuration.lookup(domain_id, domain_configuration.context);
  }
  return runtime_domain_lookup(domain_id);
}

void runtime_domain_recovered_configure(
    const RuntimeDomainRecoveredConfiguration *configuration) {
  if (configuration == NULL || (configuration->selector_nibble_count != 0 &&
                                configuration->selector_bytes == NULL)) {
    runtime_domain_recovered_trap();
  }
  domain_configuration = *configuration;
  domain_configuration_bound = true;
}

bool RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_current_domain_index_equals(uint32_t expected_index) {
  return *runtime_domain_current_index_pointer() == expected_index;
}

RuntimeDomainByteView RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_current_domain_index_bounded_view(void) {
  uint8_t *index = runtime_domain_current_index_pointer();
  return (RuntimeDomainByteView){
      .current = index,
      .lower_bound = index,
      .upper_bound = index + 1,
  };
}

uint8_t RUNTIME_DOMAIN_RECOVERED_PUBLIC runtime_current_domain_index(void) {
  return *runtime_domain_current_index_pointer();
}

RuntimeDomainView RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_resolve_object_id(uint64_t object_id) {
  if (!domain_configuration_bound ||
      domain_configuration.resolve_object == NULL) {
    runtime_domain_recovered_trap();
  }
  RuntimeDomainView domain = domain_configuration.resolve_object(
      object_id,
      sizeof(RuntimeDomain),
      domain_configuration.context
  );
  if (!runtime_domain_view_is_valid(domain)) {
    runtime_domain_recovered_trap();
  }
  return domain;
}

bool RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_registry_contains_id(uint64_t domain_id) {
  return domain_id <= RUNTIME_DOMAIN_ID_MAX &&
         runtime_domain_lookup_operation((uint8_t)domain_id) != NULL;
}

RuntimeDomainIdentityView RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_current_domain_identity_bounded_view(void) {
  uint64_t *identity = runtime_domain_current_identity_pointer();
  return (RuntimeDomainIdentityView){
      .current = identity,
      .lower_bound = identity,
      .upper_bound = identity + 1,
  };
}

RuntimeDomainHandleView RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_current_domain_handle_bounded_view(void) {
  uint16_t *handle = runtime_domain_current_handle_pointer();
  return (RuntimeDomainHandleView){
      .current = handle,
      .lower_bound = handle,
      .upper_bound = handle + 1,
  };
}

uint64_t *RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_identity_tuple_validate(RuntimeDomainIdentityView identity) {
  if (!runtime_domain_address_in_bounds(
          (uintptr_t)identity.current, (uintptr_t)identity.lower_bound,
          (uintptr_t)identity.upper_bound, sizeof(*identity.current))) {
    runtime_domain_recovered_trap();
  }
  return identity.current;
}

void RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_current_domain_metadata_publish(RuntimeDomainView domain) {
  if (!runtime_domain_view_is_valid(domain)) {
    runtime_domain_recovered_trap();
  }
  RuntimeCurrentDomainMetadata *metadata =
      runtime_domain_current_metadata_pointer();
  runtime_firebloom_copy_span(
      (RuntimeFirebloomSpan){
          .current = metadata,
          .lower_bound = metadata,
          .upper_bound = metadata + 1,
          .type = &runtime_domain_metadata_byte_type,
      },
      (RuntimeFirebloomSpan){
          .current = &domain.current->primary_virtual_base,
          .lower_bound = domain.current,
          .upper_bound = domain.current + 1,
          .type = &runtime_domain_metadata_byte_type,
      },
      sizeof(*metadata)
  );

  RuntimeDomainIdentityView identity =
      runtime_current_domain_identity_bounded_view();
  *runtime_domain_identity_tuple_validate(identity) =
      domain.current->service_key;
}

_Noreturn void RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_record_bounds_panic(void) {
  runtime_domain_recovered_trap();
}

void RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_selector_decode_nibble_position(
    uint32_t selector_position,
    RuntimeDomainWordView byte_index,
    RuntimeDomainWordView shift
) {
  if (!domain_configuration_bound ||
      selector_position >= domain_configuration.selector_nibble_count ||
      !runtime_domain_address_in_bounds(
          (uintptr_t)byte_index.current, (uintptr_t)byte_index.lower_bound,
          (uintptr_t)byte_index.upper_bound, sizeof(*byte_index.current)) ||
      !runtime_domain_address_in_bounds(
          (uintptr_t)shift.current, (uintptr_t)shift.lower_bound,
          (uintptr_t)shift.upper_bound, sizeof(*shift.current))) {
    runtime_domain_recovered_trap();
  }
  *byte_index.current = selector_position >> 1;
  *shift.current = (selector_position & 1u) << 2;
}

uint8_t RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_selector_lookup_id(uint32_t selector_position) {
  uint32_t byte_index = 0;
  uint32_t shift = 0;
  RuntimeDomainWordView byte_index_view = {
      .current = &byte_index,
      .lower_bound = &byte_index,
      .upper_bound = &byte_index + 1,
  };
  RuntimeDomainWordView shift_view = {
      .current = &shift,
      .lower_bound = &shift,
      .upper_bound = &shift + 1,
  };

  runtime_domain_selector_decode_nibble_position(
      selector_position,
      byte_index_view,
      shift_view
  );
  return (domain_configuration.selector_bytes[byte_index] >> shift) & 0x0fu;
}

uint16_t RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_public_handle_for_id(uint32_t domain_id) {
  if (domain_id == 0 || domain_id > RUNTIME_DOMAIN_ID_MAX) {
    runtime_domain_recovered_trap();
  }
  RuntimeDomain *domain = runtime_domain_lookup_operation((uint8_t)domain_id);
  if (domain == NULL || domain->public_handle == 0) {
    runtime_domain_recovered_trap();
  }
  return domain->public_handle;
}

uint16_t RUNTIME_DOMAIN_RECOVERED_PUBLIC runtime_current_domain_handle(void) {
  return *runtime_domain_current_handle_pointer();
}

RuntimeDomainView RUNTIME_DOMAIN_RECOVERED_PUBLIC
runtime_domain_resolve_wait_signal_capability_from_x10_record(
    const RuntimeDomainCapabilityRecord *record) {
  if (record == NULL) {
    runtime_domain_recovered_trap();
  }
  const uint8_t required_rights =
      RUNTIME_CAPABILITY_RIGHT_WAIT | RUNTIME_CAPABILITY_RIGHT_SIGNAL;
  RuntimeDomainView domain;
  if (domain_configuration_bound &&
      domain_configuration.resolve_capability != NULL) {
    domain = domain_configuration.resolve_capability(
        record->capability_handle,
        required_rights,
        domain_configuration.context
    );
  } else {
    RuntimeDomain *resolved = runtime_capability_resolve(
        record->capability_handle,
        RUNTIME_CAPABILITY_TYPE_DOMAIN,
        required_rights
    );
    domain = (RuntimeDomainView){
        .current = resolved,
        .lower_bound = resolved,
        .upper_bound = resolved == NULL ? NULL : resolved + 1,
    };
  }
  if (!runtime_domain_view_is_valid(domain)) {
    runtime_domain_recovered_trap();
  }
  return domain;
}

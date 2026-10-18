#include "avp/runtime/ipc/runtime_domain_recovered.h"

#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/support/runtime_state.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  RuntimeDomain domains[3];
  uint64_t resolved_object_id;
  size_t resolved_object_size;
  uint16_t resolved_capability;
  uint8_t resolved_rights;
} DomainProbe;

static DomainProbe *active_probe;
static RuntimeGlobalState fallback_state;

uint8_t runtime_execution_domain = 1;

volatile RuntimeGlobalState *runtime_state_get(void) { return &fallback_state; }

RuntimeDomain *runtime_domain_lookup(uint8_t domain_id) {
  for (size_t index = 0; index < 3; ++index) {
    if (active_probe->domains[index].domain_id == domain_id) {
      return &active_probe->domains[index];
    }
  }
  return NULL;
}

void *runtime_capability_resolve(uint16_t handle, uint8_t type,
                                 uint8_t required_rights) {
  assert(type == RUNTIME_CAPABILITY_TYPE_DOMAIN);
  assert(required_rights == 3);
  return handle == 0x321 ? &active_probe->domains[1] : NULL;
}

void *runtime_firebloom_copy_span(RuntimeFirebloomSpan destination,
                                  RuntimeFirebloomSpan source, size_t size) {
  assert(destination.current != NULL && source.current != NULL);
  return memcpy(destination.current, source.current, size);
}

static RuntimeDomainView resolve_object(uint64_t object_id, size_t object_size,
                                        void *context) {
  DomainProbe *probe = context;
  probe->resolved_object_id = object_id;
  probe->resolved_object_size = object_size;
  return (RuntimeDomainView){
      .current = &probe->domains[1],
      .lower_bound = &probe->domains[0],
      .upper_bound = &probe->domains[3],
  };
}

static RuntimeDomainView
resolve_capability(uint16_t handle, uint8_t required_rights, void *context) {
  DomainProbe *probe = context;
  probe->resolved_capability = handle;
  probe->resolved_rights = required_rights;
  return (RuntimeDomainView){
      .current = &probe->domains[2],
      .lower_bound = &probe->domains[0],
      .upper_bound = &probe->domains[3],
  };
}

static RuntimeDomain *lookup_domain(uint8_t domain_id, void *context) {
  DomainProbe *probe = context;
  for (size_t index = 0; index < 3; ++index) {
    if (probe->domains[index].domain_id == domain_id) {
      return &probe->domains[index];
    }
  }
  return NULL;
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

static void bounds_panic(void) { runtime_domain_record_bounds_panic(); }

static void invalid_identity(void) {
  uint64_t identity = 7;
  (void)runtime_domain_identity_tuple_validate((RuntimeDomainIdentityView){
      .current = &identity,
      .lower_bound = &identity,
      .upper_bound = &identity,
  });
}

static void invalid_selector(void) {
  (void)runtime_domain_selector_lookup_id(4);
}

static void invalid_public_handle(void) {
  (void)runtime_domain_public_handle_for_id(0);
}

static RuntimeDomainRecoveredConfiguration
configuration(DomainProbe *probe, uint8_t *current_index,
              uint16_t *current_handle, uint64_t *current_identity,
              RuntimeCurrentDomainMetadata *metadata,
              const uint8_t *selectors) {
  return (RuntimeDomainRecoveredConfiguration){
      .current_index = current_index,
      .current_handle = current_handle,
      .current_identity = current_identity,
      .current_metadata = metadata,
      .selector_bytes = selectors,
      .selector_nibble_count = 4,
      .resolve_object = resolve_object,
      .resolve_capability = resolve_capability,
      .lookup = lookup_domain,
      .context = probe,
  };
}

static void test_current_state_and_metadata(DomainProbe *probe) {
  uint8_t current_index = 3;
  uint16_t current_handle = 0x1234;
  uint64_t current_identity = 0;
  RuntimeCurrentDomainMetadata metadata;
  const uint8_t selectors[] = {0xa3, 0x51};
  RuntimeDomainRecoveredConfiguration config =
      configuration(probe, &current_index, &current_handle, &current_identity,
                    &metadata, selectors);
  runtime_domain_recovered_configure(&config);

  assert(runtime_current_domain_index_equals(3));
  assert(!runtime_current_domain_index_equals(4));
  assert(runtime_current_domain_index() == 3);
  RuntimeDomainByteView index_view =
      runtime_current_domain_index_bounded_view();
  assert(index_view.current == &current_index);
  assert(index_view.lower_bound == &current_index);
  assert(index_view.upper_bound == &current_index + 1);

  RuntimeDomainHandleView handle_view =
      runtime_current_domain_handle_bounded_view();
  assert(handle_view.current == &current_handle);
  assert(runtime_current_domain_handle() == 0x1234);

  RuntimeDomainIdentityView identity_view =
      runtime_current_domain_identity_bounded_view();
  assert(runtime_domain_identity_tuple_validate(identity_view) ==
         &current_identity);

  RuntimeDomain *domain = &probe->domains[1];
  domain->primary_virtual_base = UINT64_C(0xfffffc0300000000);
  domain->primary_size = 0x44000;
  domain->primary_reserved = UINT64_C(0x8877665544332211);
  memset(domain->reserved_0b8, 0x5a, sizeof(domain->reserved_0b8));
  domain->service_key = UINT64_C(0x123456789abcdef0);
  runtime_current_domain_metadata_publish((RuntimeDomainView){
      .current = domain,
      .lower_bound = &probe->domains[0],
      .upper_bound = &probe->domains[3],
  });
  assert(metadata.primary_virtual_base == domain->primary_virtual_base);
  assert(metadata.primary_size == domain->primary_size);
  assert(metadata.primary_reserved == domain->primary_reserved);
  assert(memcmp(metadata.reserved_18, domain->reserved_0b8,
                sizeof(metadata.reserved_18)) == 0);
  assert(current_identity == domain->service_key);
}

static void test_selector_registry_and_handles(DomainProbe *probe) {
  uint8_t current_index = 3;
  uint16_t current_handle = 0x1234;
  uint64_t current_identity = 0;
  RuntimeCurrentDomainMetadata metadata;
  const uint8_t selectors[] = {0xa3, 0x51};
  RuntimeDomainRecoveredConfiguration config =
      configuration(probe, &current_index, &current_handle, &current_identity,
                    &metadata, selectors);
  runtime_domain_recovered_configure(&config);

  uint32_t byte_index = UINT32_MAX;
  uint32_t shift = UINT32_MAX;
  runtime_domain_selector_decode_nibble_position(
      3,
      (RuntimeDomainWordView){.current = &byte_index,
                              .lower_bound = &byte_index,
                              .upper_bound = &byte_index + 1},
      (RuntimeDomainWordView){
          .current = &shift, .lower_bound = &shift, .upper_bound = &shift + 1});
  assert(byte_index == 1 && shift == 4);
  assert(runtime_domain_selector_lookup_id(0) == 3);
  assert(runtime_domain_selector_lookup_id(1) == 10);
  assert(runtime_domain_selector_lookup_id(2) == 1);
  assert(runtime_domain_selector_lookup_id(3) == 5);

  assert(runtime_domain_registry_contains_id(1));
  assert(runtime_domain_registry_contains_id(3));
  assert(!runtime_domain_registry_contains_id(4));
  assert(!runtime_domain_registry_contains_id(16));
  assert(runtime_domain_public_handle_for_id(3) == 0x303);
}

static void test_resolvers(DomainProbe *probe) {
  uint8_t current_index = 3;
  uint16_t current_handle = 0x1234;
  uint64_t current_identity = 0;
  RuntimeCurrentDomainMetadata metadata;
  const uint8_t selectors[] = {0xa3, 0x51};
  RuntimeDomainRecoveredConfiguration config =
      configuration(probe, &current_index, &current_handle, &current_identity,
                    &metadata, selectors);
  runtime_domain_recovered_configure(&config);

  RuntimeDomainView domain =
      runtime_domain_resolve_object_id(UINT64_C(0x55667788));
  assert(domain.current == &probe->domains[1]);
  assert(probe->resolved_object_id == UINT64_C(0x55667788));
  assert(probe->resolved_object_size == sizeof(RuntimeDomain));

  RuntimeDomainCapabilityRecord record = {.capability_handle = 0x987};
  domain =
      runtime_domain_resolve_wait_signal_capability_from_x10_record(&record);
  assert(domain.current == &probe->domains[2]);
  assert(probe->resolved_capability == 0x987);
  assert(probe->resolved_rights == 3);
}

int main(void) {
  DomainProbe probe;
  memset(&probe, 0, sizeof(probe));
  active_probe = &probe;
  probe.domains[0].domain_id = 1;
  probe.domains[0].public_handle = 0x101;
  probe.domains[1].domain_id = 3;
  probe.domains[1].public_handle = 0x303;
  probe.domains[2].domain_id = 7;
  probe.domains[2].public_handle = 0x707;

  test_current_state_and_metadata(&probe);
  test_selector_registry_and_handles(&probe);
  test_resolvers(&probe);
  expect_trap(bounds_panic);
  expect_trap(invalid_identity);
  expect_trap(invalid_selector);
  expect_trap(invalid_public_handle);
  return 0;
}

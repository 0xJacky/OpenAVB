#include "avp/runtime/memory/runtime_physical_memory_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define REGION_DESCRIPTOR(id, mapping_attribute, flags)                   \
  ((uint64_t)(id) | ((uint64_t)(mapping_attribute) << UINT32_C(16)) | \
   ((uint64_t)(flags) << UINT32_C(32)))

typedef struct {
  size_t map_calls;
  size_t unmap_calls;
  uint16_t region_id;
  uintptr_t physical_base;
  uintptr_t active_alias;
  size_t size;
  uint32_t mapping_attribute;
  uint32_t flags;
  bool supervisor_services;
} PhysicalMemoryProbe;

static PhysicalMemoryProbe probe;
static RuntimePhysicalMemoryRegion registry_records[3];
static uint8_t policy_classes[RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT];

_Noreturn void runtime_firebloom_panic(void) {
  __builtin_trap();
  __builtin_unreachable();
}

void runtime_firebloom_validate_span(const RuntimeFirebloomSpan *span,
                                     size_t size) {
  if (span == NULL) {
    runtime_firebloom_panic();
  }
  const uintptr_t current = (uintptr_t)span->current;
  const uintptr_t lower = (uintptr_t)span->lower_bound;
  const uintptr_t upper = (uintptr_t)span->upper_bound;
  if (span->type == NULL || current < lower || current >= upper ||
      size > upper - current) {
    runtime_firebloom_panic();
  }
}

void *runtime_firebloom_fill_span(RuntimeFirebloomSpan destination, int value,
                                  size_t size) {
  runtime_firebloom_validate_span(&destination, size);
  return memset(destination.current, value, size);
}

static uintptr_t map_alias(uint16_t region_id, uintptr_t physical_base,
                           size_t size, uint32_t mapping_attribute,
                           uint32_t flags, bool supervisor_services,
                           void *context) {
  PhysicalMemoryProbe *active_probe = context;
  ++active_probe->map_calls;
  active_probe->region_id = region_id;
  active_probe->physical_base = physical_base;
  active_probe->size = size;
  active_probe->mapping_attribute = mapping_attribute;
  active_probe->flags = flags;
  active_probe->supervisor_services = supervisor_services;
  return physical_base + UINT32_C(0x100000);
}

static void unmap_alias(uint16_t region_id, uintptr_t active_alias,
                        size_t size, uint32_t flags,
                        bool supervisor_services, void *context) {
  PhysicalMemoryProbe *active_probe = context;
  ++active_probe->unmap_calls;
  active_probe->region_id = region_id;
  active_probe->active_alias = active_alias;
  active_probe->size = size;
  active_probe->flags = flags;
  active_probe->supervisor_services = supervisor_services;
}

static void configure(bool supervisor_services) {
  memset(&probe, 0, sizeof(probe));
  memset(policy_classes, 0, sizeof(policy_classes));
  policy_classes[0x2d] = 1;
  policy_classes[0xa6] = 2;
  RuntimePhysicalMemoryRecoveredConfiguration configuration = {
      .policy_classes = policy_classes,
      .policy_class_count = RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT,
      .policy_class_limit = 2,
      .uses_supervisor_services = supervisor_services,
      .map_alias = map_alias,
      .unmap_alias = unmap_alias,
      .context = &probe,
  };
  runtime_physical_memory_recovered_configure(&configuration);
}

static RuntimeFirebloomSpan lookup(uint16_t region_id) {
  RuntimeFirebloomSpan span =
      runtime_physical_memory_region_lookup(region_id);
  assert(span.current != NULL);
  return span;
}

static void initialize_registry(void) {
  memset(registry_records, 0, sizeof(registry_records));
  registry_records[0] = (RuntimePhysicalMemoryRegion){
      .physical_base = 0x70064000,
      .size = 0x8000,
      .descriptor = REGION_DESCRIPTOR(0xa6, 0, 0),
  };
  registry_records[1] = (RuntimePhysicalMemoryRegion){
      .physical_base = 0x70074000,
      .active_alias = 0x70074000,
      .size = 0x10000,
      .descriptor = REGION_DESCRIPTOR(0x2d, 0, 0),
  };
  registry_records[2] = (RuntimePhysicalMemoryRegion){
      .physical_base = UINTPTR_MAX,
      .size = SIZE_MAX,
      .descriptor = REGION_DESCRIPTOR(0x103, 0, 0),
  };
  runtime_physical_memory_registry_initialize(
      (RuntimePhysicalMemoryRegistry){
          .records = registry_records,
          .count = sizeof(registry_records) / sizeof(registry_records[0]),
      });
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

static void alignment_panic(void) {
  runtime_physical_memory_region_alignment_bounds_panic();
}

static void active_base_panic(void) {
  runtime_physical_memory_active_base_bounds_panic();
}

static void defined_panic(void) {
  runtime_physical_memory_region_is_defined_bounds_panic();
}

static void invalid_policy_id(void) {
  (void)runtime_physical_memory_region_policy_class(0x27e);
}

static void invalid_mapping_attribute(void) {
  (void)runtime_physical_memory_region_activate_alias(lookup(0xa6), 23);
}

static void duplicate_apply(void) {
  RuntimePhysicalMemoryRegion record = {
      .physical_base = 0x71100000,
      .size = 0x4000,
      .descriptor = REGION_DESCRIPTOR(0xa6, 0, 0),
  };
  (void)runtime_physical_memory_apply_region_records(&record,
                                                     sizeof(record));
}

static void test_alignment_liveness_and_tuple_contracts(void) {
  RuntimeFirebloomSpan aligned = lookup(0xa6);
  assert(runtime_physical_memory_region_is_16k_aligned(aligned));
  assert(j_runtime_physical_memory_region_is_live(aligned));
  assert(j_runtime_physical_memory_region_is_live_clone(aligned));
  assert(runtime_physical_memory_outlined_save_region_tuple_and_test_live(
      aligned));

  RuntimeFirebloomSpan restored =
      runtime_physical_memory_outlined_restore_region_tuple_x22_x19(aligned);
  RuntimeFirebloomSpan saved =
      runtime_physical_memory_outlined_save_region_tuple_x19_x21_x20(aligned);
  assert(memcmp(&restored, &aligned, sizeof(aligned)) == 0);
  assert(memcmp(&saved, &aligned, sizeof(aligned)) == 0);

  registry_records[0].size = 0x4100;
  assert(!runtime_physical_memory_region_is_16k_aligned(aligned));
  registry_records[0].size = 0x8000;
  assert(!j_runtime_physical_memory_region_is_live(lookup(0x103)));
}

static void test_policy_and_registry_views(void) {
  assert(runtime_physical_memory_region_policy_class(0x2d) == 1);
  assert(runtime_physical_memory_region_policy_class(0xa6) == 2);
  assert(runtime_physical_memory_uses_supervisor_services());

  RuntimePhysicalMemoryCountView count_view =
      runtime_physical_memory_region_count_span();
  assert(count_view.current == count_view.lower_bound);
  assert(count_view.upper_bound == count_view.current + 1);
  assert(*count_view.current == RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT);

  RuntimePhysicalMemoryConstByteView limit_view =
      runtime_physical_memory_region_policy_class_limit_span();
  assert(limit_view.current == limit_view.lower_bound);
  assert(limit_view.upper_bound == limit_view.current + 1);
  assert(*limit_view.current == 2);

  RuntimePhysicalMemoryConstByteView table_view =
      runtime_physical_memory_region_policy_class_table_span();
  assert(table_view.current[0x2d] == 1);
  assert(table_view.current[0xa6] == 2);
  assert((size_t)(table_view.upper_bound - table_view.lower_bound) ==
         RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT);

  RuntimePhysicalMemoryRegionView registry_view;
  uint32_t registry_count = 0;
  runtime_physical_memory_initialize_fixed_registry_view(&registry_view,
                                                         &registry_count);
  assert(registry_view.current == registry_records);
  assert(registry_view.lower_bound == registry_records);
  assert(registry_view.upper_bound == registry_records + 3);
  assert(registry_count == 3);

  RuntimePhysicalMemoryRegionView record_view =
      runtime_physical_memory_region_bounds_from_x23(&registry_records[0]);
  assert(record_view.current == &registry_records[0]);
  assert(record_view.lower_bound == &registry_records[0]);
  assert((uintptr_t)record_view.upper_bound ==
         (uintptr_t)&registry_records[0] + 0x20);
}

static void test_alias_lifecycle(void) {
  RuntimeFirebloomSpan span = lookup(0xa6);
  RuntimePhysicalMemoryAliasView alias =
      runtime_physical_memory_region_activate_alias_with_flags(span, 31, 5);
  assert(alias.current == 0x70164000);
  assert(alias.lower_bound == alias.current);
  assert(alias.upper_bound == alias.current + 0x8000);
  assert(registry_records[0].active_alias == alias.current);
  assert(((registry_records[0].descriptor >> 16) & UINT16_MAX) == 31);
  assert(probe.map_calls == 1);
  assert(probe.region_id == 0xa6);
  assert(probe.mapping_attribute == 31);
  assert(probe.flags == 5);
  assert(probe.supervisor_services);

  runtime_physical_memory_region_deactivate_alias(span, 7);
  assert(registry_records[0].active_alias == 0);
  assert(((registry_records[0].descriptor >> 16) & UINT16_MAX) == 0);
  assert(probe.unmap_calls == 1);
  assert(probe.active_alias == alias.current);
  assert(probe.flags == 7);

  alias = runtime_physical_memory_region_activate_alias(span, 24);
  assert(alias.current == 0x70164000);
  assert(probe.flags == 0);
}

static void test_apply_records_and_initialized_state(void) {
  configure(false);
  initialize_registry();
  registry_records[0].metadata.current = (void *)(uintptr_t)0x1234;

  RuntimePhysicalMemoryRegion records[3] = {
      {
          .physical_base = 0x71000000,
          .size = 0x8000,
          .descriptor = REGION_DESCRIPTOR(0xa6, 30, 0x8000),
      },
      {
          .physical_base = 0x72000000,
          .size = 0x4000,
          .descriptor = REGION_DESCRIPTOR(0x2d, 0, 0x8001),
      },
      {
          .physical_base = 0x73000000,
          .size = 0x4000,
          .descriptor = REGION_DESCRIPTOR(0x2d, 0, 0x8000),
      },
  };
  const uintptr_t result = runtime_physical_memory_apply_region_records(
      records, sizeof(records));
  assert(result == 0x73000000);
  assert(registry_records[0].physical_base == 0x71000000);
  assert(registry_records[0].active_alias == 0x71100000);
  assert(registry_records[0].size == 0x8000);
  assert(registry_records[0].metadata.current == (void *)(uintptr_t)0x1234);
  assert(registry_records[1].physical_base == 0x73000000);
  assert(registry_records[1].size == 0x4000);
  assert(probe.map_calls == 1);
  assert(!probe.supervisor_services);

  RuntimePhysicalMemoryByteView initialized =
      runtime_physical_memory_regions_initialized_flag_bounded_view();
  assert(initialized.current == initialized.lower_bound);
  assert(initialized.upper_bound == initialized.current + 1);
  assert(*initialized.current == 1);
  expect_trap(duplicate_apply);
}

int main(void) {
  configure(true);
  initialize_registry();
  test_alignment_liveness_and_tuple_contracts();
  test_policy_and_registry_views();
  test_alias_lifecycle();
  expect_trap(alignment_panic);
  expect_trap(active_base_panic);
  expect_trap(defined_panic);
  expect_trap(invalid_policy_id);
  expect_trap(invalid_mapping_attribute);
  test_apply_records_and_initialized_state();
  return 0;
}

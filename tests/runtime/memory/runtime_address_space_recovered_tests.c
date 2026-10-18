#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/memory/runtime_address_space_recovered.h"
#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/memory/runtime_physical_memory.h"
#include "avp/runtime/support/runtime_state.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static RuntimePhysicalMemoryRegion physical_region;
static RuntimePageTablePage page_table_pages[16];
static RuntimePageTablePool page_table_pool;
static RuntimePageTable page_table;
static RuntimeDomainRegistryLink domain_link;
static RuntimeAddressSpaceRecoveredPhysicalPolicy physical_policy;
static RuntimeAddressSpaceRecoveredContext recovered;

void runtime_page_table_arch_publish(void) {}

void runtime_page_table_arch_invalidate_all(void) {}

_Noreturn void pvpanic_trigger(void) { __builtin_trap(); }

static void initialize_runtime(void) {
  runtime_state_initialize();
  runtime_capability_initialize();
  runtime_domain_reset(AVP_RUNTIME_DOMAIN_BOOT);
  runtime_address_space_initialize();
  physical_region = (RuntimePhysicalMemoryRegion){
      .physical_base = UINT64_C(0x200000),
      .active_alias = UINT64_C(0x60200000),
      .size = UINT64_C(0x8000),
      .descriptor = (UINT64_C(0x80000009) << 16) | UINT64_C(0x00a6),
  };
  runtime_physical_memory_registry_initialize((RuntimePhysicalMemoryRegistry){
      .records = &physical_region,
      .count = 1,
  });
  assert(runtime_page_table_pool_initialize(&page_table_pool, page_table_pages,
                                            UINT64_C(0x10000000), 16) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_initialize(&page_table, &page_table_pool, 42, 0) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  runtime_address_space_bind_page_table(&page_table);

  domain_link = (RuntimeDomainRegistryLink){0};
  physical_policy = (RuntimeAddressSpaceRecoveredPhysicalPolicy){
      .lower = UINT64_C(0x200000),
      .upper = UINT64_C(0x300000),
      .region_id = UINT16_C(0x00a6),
  };
  recovered = (RuntimeAddressSpaceRecoveredContext){
      .page_table = &page_table,
      .physical_policies = &physical_policy,
      .physical_policy_count = 1,
      .domain_count = 1,
      .profile_slide = UINT64_C(0x60000000),
      .mapped_allocation_lower = UINT64_C(0x10000000),
      .mapped_allocation_upper = UINT64_C(0x11000000),
      .domain_local_base = UINT64_C(0xfffffc0000000000),
      .domain_link_type = UINT64_C(0x145000),
      .current_domain = AVP_RUNTIME_DOMAIN_BOOT,
      .profile_direct_map = true,
  };
  recovered.domains[0] = (RuntimeAddressSpaceRecoveredDomainLink){
      .domain_id = AVP_RUNTIME_DOMAIN_BOOT,
      .link = &domain_link,
  };
  runtime_address_space_recovered_bind_context(&recovered);
}

static RuntimeAddressSpaceMapResult map_global(void) {
  RuntimeAddressSpaceMapResult result =
      runtime_address_space_map_global_direct_range(
          UINT64_C(0x200000), UINT64_C(0x8000), UINT16_C(0x00a6), 3, 1, 7);
  assert(result.mapping_handle != 0);
  assert(result.virtual_lower_bound == UINT64_C(0x60200000));
  return result;
}

static void test_profile_and_outliners(void) {
  initialize_runtime();
  assert(runtime_address_space_uses_profile_direct_map());
  assert(runtime_address_profile_load_slide_to_x8() == UINT64_C(0x60000000));
  RuntimeAddressSpaceRecoveredProfileDelta delta =
      runtime_address_profile_outlined_load_slide_and_domain_local_delta();
  assert(delta.slide == UINT64_C(0x60000000));
  assert(delta.domain_local_delta == UINT64_C(0xfffffcf000000000));

  assert(runtime_address_space_address_in_mapped_allocation_window(
      UINT64_C(0x70000000)));
  assert(!runtime_address_space_address_in_mapped_allocation_window(
      UINT64_C(0x71000000)));
  const uintptr_t local_base =
      runtime_address_space_outlined_compute_domain_local_window_base(
          AVP_RUNTIME_DOMAIN_BOOT);
  assert(runtime_address_profile_outlined_finish_domain_local_window_test(
      local_base + UINT64_C(0x1000), local_base));
  assert(runtime_address_space_address_is_domain_local(local_base +
                                                       UINT64_C(0x1000)));

  RuntimeAddressSpaceRecoveredMappingArguments arguments =
      runtime_address_space_outlined_prepare_mapping_arguments(1, 2, 3, 4, 5,
                                                               6);
  assert(arguments.virtual_address == 1);
  assert(arguments.physical_address == 2);
  assert(arguments.size == 3);
  assert(arguments.regime == 4);
  assert(arguments.memory_attribute == 5);
  assert(arguments.flags == 6);

  RuntimeAddressSpaceRecoveredDomainLinkSlots slots =
      runtime_address_space_outlined_load_current_domain_link_slots();
  assert(slots.current == &recovered.domains[0]);
  assert(slots.lower == &recovered.domains[0]);
  assert(slots.upper == &recovered.domains[1]);
  assert(slots.type == UINT64_C(0x145000));
  assert(runtime_address_space_outlined_resolve_domain_link_x26(
             slots.current) == &domain_link);

  RuntimeAddressSpaceRecoveredOptionalMappingState optional =
      runtime_address_space_outlined_clear_optional_mapping_state();
  assert(optional.direct_mapping == 0);
  assert(optional.owner_lower == 0);
  assert(optional.owner_upper == 0);
  assert(optional.owner_type == 0);
}

static void test_bitmap_and_physical_policy(void) {
  initialize_runtime();
  RuntimeAddressSpaceRecoveredBitmapSpan span =
      runtime_address_space_attached_region_bitmap_span();
  assert(span.current == recovered.attached_region_bitmap);
  assert(span.lower == span.current);
  assert(span.upper - span.lower == 80);
  assert(span.type == UINT64_C(0x14a860));
  span =
      runtime_address_space_outlined_prepare_attached_region_bitmap_arguments(
          UINT16_C(0x00a6));
  assert(span.current == recovered.attached_region_bitmap);

  assert(runtime_address_space_physical_range_is_global_direct(
      UINT64_C(0x200000), UINT64_C(0x8000), UINT16_C(0x00a6)));
  physical_policy.reserved = true;
  assert(!runtime_address_space_physical_range_is_global_direct(
      UINT64_C(0x200000), UINT64_C(0x8000), UINT16_C(0x00a6)));
  physical_policy.reserved = false;
  physical_policy.excluded_set_mask = 1;
  assert(!runtime_address_space_physical_range_is_global_direct(
      UINT64_C(0x200000), UINT64_C(0x8000), UINT16_C(0x00a6)));
}

static void test_global_mapping_and_resolution(void) {
  initialize_runtime();
  RuntimeAddressSpaceMapResult result = map_global();
  RuntimeAddressSpaceMapping *mapping =
      runtime_address_space_mapping_resolve(result.mapping_handle, 7);
  assert(mapping->virtual_lower_bound == UINT64_C(0x60200000));
  assert(mapping->virtual_upper_bound == UINT64_C(0x60208000));
  assert(mapping->physical_base == UINT64_C(0x200000));
  assert(mapping->flags & RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL);

  const uint32_t *entry =
      runtime_capability_entry_lookup(result.mapping_handle);
  assert(entry != 0);
  const uint16_t object_id = runtime_capability_object_id(*entry);
  assert(runtime_address_space_mapping_resolve_object_id(object_id) == mapping);

  RuntimePageTableTranslation translation;
  assert(runtime_page_table_lookup(&page_table, UINT64_C(0x60204000),
                                   &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(translation.physical_address == UINT64_C(0x204000));

  RuntimeFirebloomSpan region =
      runtime_physical_memory_region_lookup(UINT16_C(0x00a6));
  assert(runtime_address_space_outlined_validate_saved_region_mapping(
      region, mapping, AVP_RUNTIME_DOMAIN_BOOT));
}

static void test_attach_detach_and_unmap(void) {
  initialize_runtime();
  RuntimeAddressSpaceMapResult source = map_global();
  RuntimeAddressSpaceMapResult derived =
      runtime_address_space_attach_mapping_to_domain(
          source.mapping_handle, AVP_RUNTIME_DOMAIN_BOOT,
          RUNTIME_ADDRESS_SPACE_PAGE_SIZE, RUNTIME_ADDRESS_SPACE_PAGE_SIZE, 3);
  assert(derived.mapping_handle != 0);
  const size_t bitmap_index = UINT16_C(0x00a6) >> 3;
  const uint8_t bitmap_mask = (uint8_t)(1u << (UINT16_C(0x00a6) & 7));
  assert(recovered.attached_region_bitmap[bitmap_index] & bitmap_mask);
  assert(runtime_address_space_detach_mapping_from_domain(
             derived.mapping_handle, AVP_RUNTIME_DOMAIN_BOOT) ==
         RUNTIME_ADDRESS_SPACE_RECOVERED_OK);

  assert(runtime_address_space_unmap_current_range(UINT64_C(0x60200000),
                                                   UINT64_C(0x8000)) ==
         RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  RuntimePageTableTranslation translation;
  assert(runtime_page_table_lookup(&page_table, UINT64_C(0x60200000),
                                   &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
}

static void test_shared_task_page_and_mode_unmap(void) {
  initialize_runtime();
  RuntimeAddressSpaceRecoveredTask task = {
      .shared_virtual_base = UINT64_C(0x70400000),
      .shared_physical_base = UINT64_C(0x400000),
      .domain_id = AVP_RUNTIME_DOMAIN_BOOT,
  };
  assert(runtime_task_ensure_shared_page_mapping(&task, 3, 1) ==
         UINT64_C(0x70400000));
  assert(task.mapping_handle != 0);
  assert(runtime_task_ensure_shared_page_mapping(&task, 3, 1) ==
         UINT64_C(0x70400000));
  assert(runtime_address_space_unmap(task.mapping_handle,
                                     AVP_RUNTIME_DOMAIN_BOOT,
                                     1) == RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  RuntimePageTableTranslation translation;
  assert(runtime_page_table_lookup(&page_table, UINT64_C(0x70400000),
                                   &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
}

static void expect_fatal(void (*operation)(void)) {
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _Exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
}

static void audience_bounds_fatal(void) {
  runtime_address_space_mapping_audience_bounds_panic();
}

static void line_fatal(void) { runtime_address_space_line_10018_fatal(); }

int main(void) {
  test_profile_and_outliners();
  test_bitmap_and_physical_policy();
  test_global_mapping_and_resolution();
  test_attach_detach_and_unmap();
  test_shared_task_page_and_mode_unmap();
  expect_fatal(audience_bounds_fatal);
  expect_fatal(line_fatal);
  puts("runtime_address_space_recovered_tests: PASS");
  return 0;
}

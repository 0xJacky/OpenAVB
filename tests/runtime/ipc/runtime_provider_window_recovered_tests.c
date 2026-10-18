#include "avp/runtime/ipc/runtime_provider_window_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  RuntimeProviderStorageWindow storage;
  size_t allocation_size;
  size_t allocation_alignment;
  size_t raw_calls[3];
  size_t byte_offset;
  size_t byte_count;
  uintptr_t arguments[3];
  AVPRuntimeProvider *registered_provider;
  uint64_t registered_owner;
  void *registered_implementation;
  uintptr_t registered_lower;
  uintptr_t registered_upper;
  uintptr_t registered_cookie;
  size_t publish_calls;
} ProviderWindowProbe;

static ProviderWindowProbe probe;
static RuntimeProviderStorageBacking backing;

void runtime_provider_register(AVPRuntimeProvider *provider,
                               uint64_t owner_domain, const char *lookup_key,
                               const char *metadata_a, const char *metadata_b,
                               void *implementation,
                               uintptr_t implementation_lower_bound,
                               uintptr_t implementation_upper_bound,
                               uintptr_t implementation_cookie) {
  assert(lookup_key != NULL && metadata_a != NULL && metadata_b != NULL);
  probe.registered_provider = provider;
  probe.registered_owner = owner_domain;
  probe.registered_implementation = implementation;
  probe.registered_lower = implementation_lower_bound;
  probe.registered_upper = implementation_upper_bound;
  probe.registered_cookie = implementation_cookie;
}

int runtime_provider_publish(void *implementation) {
  assert(implementation == probe.registered_implementation);
  ++probe.publish_calls;
  return 0;
}

static bool raw_slot0(void *context, size_t byte_offset, size_t byte_count) {
  ProviderWindowProbe *active_probe = context;
  ++active_probe->raw_calls[0];
  active_probe->byte_offset = byte_offset;
  active_probe->byte_count = byte_count;
  return true;
}

static bool raw_slot2(void *context, size_t byte_offset, size_t byte_count) {
  ProviderWindowProbe *active_probe = context;
  ++active_probe->raw_calls[1];
  active_probe->byte_offset = byte_offset;
  active_probe->byte_count = byte_count;
  return true;
}

static bool raw_slot4(void *context, uintptr_t argument0, uintptr_t argument1,
                      uintptr_t argument2, size_t byte_offset,
                      size_t byte_count) {
  ProviderWindowProbe *active_probe = context;
  ++active_probe->raw_calls[2];
  active_probe->arguments[0] = argument0;
  active_probe->arguments[1] = argument1;
  active_probe->arguments[2] = argument2;
  active_probe->byte_offset = byte_offset;
  active_probe->byte_count = byte_count;
  return true;
}

static RuntimeProviderStorageWindowView
allocate_window(size_t size, size_t alignment, void *context) {
  ProviderWindowProbe *active_probe = context;
  active_probe->allocation_size = size;
  active_probe->allocation_alignment = alignment;
  memset(&active_probe->storage, 0, sizeof(active_probe->storage));
  return (RuntimeProviderStorageWindowView){
      .current = &active_probe->storage,
      .lower_bound = &active_probe->storage,
      .upper_bound = &active_probe->storage + 1,
  };
}

static RuntimeProviderStorageBackingView backing_view(void) {
  return (RuntimeProviderStorageBackingView){
      .current = &backing,
      .lower_bound = &backing,
      .upper_bound = &backing + 1,
  };
}

static RuntimeProviderStorageWindowView create_window(void) {
  memset(&probe, 0, sizeof(probe));
  backing = (RuntimeProviderStorageBacking){
      .context = &probe,
      .operations =
          {
              .slot0 = raw_slot0,
              .slot2 = raw_slot2,
              .slot4 = raw_slot4,
          },
  };
  return runtime_provider_storage_window_create(
      allocate_window, &probe, backing_view(), 0x1000, 0x280, 4);
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

static void invalid_window_tuple(void) {
  RuntimeProviderStorageWindow window = {0};
  (void)runtime_provider_storage_window_validate_tuple(
      (RuntimeProviderStorageWindowView){
          .current = &window,
          .lower_bound = &window,
          .upper_bound = &window + 1,
      });
}

static void invalid_allocated_tuple(void) {
  RuntimeProviderStorageWindow window = {0};
  (void)runtime_provider_storage_window_validate_allocated_tuple(
      (RuntimeProviderStorageWindowView){
          .current = &window,
          .lower_bound = &window,
          .upper_bound = &window,
      });
}

static void negative_offset(void) {
  RuntimeProviderStorageWindowView window = create_window();
  (void)runtime_provider_storage_window_slot0_forward(window.current, SIZE_MAX,
                                                      1);
}

static void missing_operation(void) {
  RuntimeProviderStorageWindowView window = create_window();
  backing.operations.slot2 = NULL;
  (void)runtime_provider_storage_window_slot2_forward(window.current, 0, 1);
}

static void test_create_initialize_and_views(void) {
  RuntimeProviderStorageWindowView window = create_window();
  assert(probe.allocation_size == RUNTIME_PROVIDER_STORAGE_WINDOW_SIZE);
  assert(probe.allocation_alignment == _Alignof(RuntimeProviderStorageWindow));
  assert(runtime_provider_storage_window_validate_tuple(window) ==
         window.current);
  assert(runtime_provider_storage_window_validate_allocated_tuple(window) ==
         window.current);
  assert(window.current->initialized == 1);
  assert(window.current->element_shift == 4);
  assert(window.current->byte_length == 0x280);
  assert(window.current->backing == &backing);
  assert(window.current->backing_byte_offset == 0x1000);
  assert(window.current->slot0 ==
         runtime_provider_storage_window_slot0_forward);
  assert(window.current->slot1 ==
         runtime_provider_storage_window_slot1_forward);
  assert(window.current->slot2 ==
         runtime_provider_storage_window_slot2_forward);
  assert(window.current->slot3 ==
         runtime_provider_storage_window_slot3_forward);
  assert(window.current->slot4 ==
         runtime_provider_storage_window_slot4_forward);

  RuntimeProviderStorageWindowView duplicate =
      runtime_provider_storage_window_duplicate_current_lower(window);
  assert(duplicate.current == window.current);
  assert(duplicate.lower_bound == window.current);
  assert(duplicate.upper_bound == window.upper_bound);

  RuntimeProviderStorageBackingView descriptor =
      runtime_provider_storage_window_backing_descriptor_tuple(&backing);
  assert(descriptor.current == &backing);
  assert(descriptor.lower_bound == &backing);
  assert(descriptor.upper_bound == &backing + 1);
  assert(
      runtime_provider_storage_window_save_secondary_tuple_x22_x19(descriptor)
          .current == &backing);
  assert(runtime_provider_storage_window_restore_secondary_tuple_x22_x19(
             descriptor)
             .current == &backing);
  assert(runtime_provider_storage_window_restore_saved_primary_to_secondary(
             descriptor)
             .current == &backing);

  RuntimeProviderStorageCallbackBinding binding =
      runtime_provider_storage_window_prepare_callback_binding(window, &probe);
  assert(binding.object.current == window.current);
  assert(binding.callback == &probe);
  assert(runtime_provider_storage_window_source_token() ==
         RUNTIME_PROVIDER_STORAGE_WINDOW_SOURCE_TOKEN);
}

static void test_forwarding_and_clipping(void) {
  RuntimeProviderStorageWindowView window = create_window();
  assert(runtime_provider_storage_window_slot0_forward(window.current, 0x20,
                                                       0x30));
  assert(probe.raw_calls[0] == 1);
  assert(probe.byte_offset == 0x1020);
  assert(probe.byte_count == 0x30);

  assert(runtime_provider_storage_window_slot0_forward(window.current, 0x260,
                                                       0x80));
  assert(probe.byte_offset == 0x1260);
  assert(probe.byte_count == 0x20);
  assert(
      !runtime_provider_storage_window_slot0_forward(window.current, 0x280, 1));
  assert(!runtime_provider_storage_window_slot0_forward(window.current, 1,
                                                        SIZE_MAX));

  assert(runtime_provider_storage_window_slot1_forward(window.current, 3, 2));
  assert(probe.raw_calls[0] == 3);
  assert(probe.byte_offset == 0x1030);
  assert(probe.byte_count == 0x20);

  assert(runtime_provider_storage_window_slot2_forward(window.current, 0x40,
                                                       0x10));
  assert(probe.raw_calls[1] == 1);
  assert(probe.byte_offset == 0x1040);
  assert(probe.byte_count == 0x10);

  assert(runtime_provider_storage_window_slot3_forward(window.current, 5, 3));
  assert(probe.raw_calls[1] == 2);
  assert(probe.byte_offset == 0x1050);
  assert(probe.byte_count == 0x30);

  assert(runtime_provider_storage_window_slot4_forward(
      window.current, 0xaa, 0xbb, 0xcc, 0x270, 0x40));
  assert(probe.raw_calls[2] == 1);
  assert(probe.arguments[0] == 0xaa);
  assert(probe.arguments[1] == 0xbb);
  assert(probe.arguments[2] == 0xcc);
  assert(probe.byte_offset == 0x1270);
  assert(probe.byte_count == 0x10);

  RuntimeProviderStorageTransfer transfer =
      runtime_provider_storage_window_scale_transfer_by_element_shift(
          window.current, 7, 9);
  assert(transfer.byte_offset == 0x70);
  assert(transfer.byte_count == 0x90);
}

static void test_registration_bridge(void) {
  memset(&probe, 0, sizeof(probe));
  backing = (RuntimeProviderStorageBacking){
      .context = &probe,
      .operations =
          {
              .slot0 = raw_slot0,
              .slot2 = raw_slot2,
              .slot4 = raw_slot4,
          },
  };
  AVPRuntimeProvider provider = {0};
  RuntimeProviderStorageWindowView window =
      runtime_provider_register_storage_window(
          &provider, 7, "storage-window", "firmware", "typed-storage",
          allocate_window, &probe, backing_view(), 0x2000, 0x400, 9);
  assert(probe.registered_provider == &provider);
  assert(probe.registered_owner == 7);
  assert(probe.registered_implementation == window.current);
  assert(probe.registered_lower == (uintptr_t)window.lower_bound);
  assert(probe.registered_upper == (uintptr_t)window.upper_bound);
  assert(probe.registered_cookie ==
         RUNTIME_PROVIDER_STORAGE_WINDOW_SOURCE_TOKEN);
  assert(probe.publish_calls == 1);

  memset(&probe, 0, sizeof(probe));
  backing.context = &probe;
  window = runtime_provider_register_storage_window_dispatch_bridge(
      &provider, 9, "storage-bridge", "firmware", "typed-storage",
      allocate_window, &probe, backing_view(), 0x3000, 0x500, 8);
  assert(window.current == &probe.storage);
  assert(probe.registered_owner == 9);
  assert(probe.publish_calls == 1);
}

int main(void) {
  test_create_initialize_and_views();
  test_forwarding_and_clipping();
  test_registration_bridge();
  expect_trap(invalid_window_tuple);
  expect_trap(invalid_allocated_tuple);
  expect_trap(negative_offset);
  expect_trap(missing_operation);
  return 0;
}

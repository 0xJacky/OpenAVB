#include "avp/runtime/ipc/runtime_provider_window_recovered.h"

#include <limits.h>

#define RUNTIME_PROVIDER_STORAGE_WINDOW_SIGNATURE UINT32_C(0xb84b0b42)

static _Noreturn void runtime_provider_storage_window_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static bool runtime_provider_storage_address_in_bounds(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
  return current != 0 && lower_bound != 0 && upper_bound != 0 &&
         current >= lower_bound && current < upper_bound &&
         size <= upper_bound - current;
}

static RuntimeProviderStorageBacking *runtime_provider_storage_backing_validate(
    RuntimeProviderStorageBackingView backing) {
  if (!runtime_provider_storage_address_in_bounds(
          (uintptr_t)backing.current, (uintptr_t)backing.lower_bound,
          (uintptr_t)backing.upper_bound,
          RUNTIME_PROVIDER_STORAGE_BACKING_SIZE)) {
    runtime_provider_storage_window_trap();
  }
  return backing.current;
}

RuntimeProviderStorageWindow *RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_validate_allocated_tuple(
    RuntimeProviderStorageWindowView window) {
  if (!runtime_provider_storage_address_in_bounds(
          (uintptr_t)window.current, (uintptr_t)window.lower_bound,
          (uintptr_t)window.upper_bound,
          RUNTIME_PROVIDER_STORAGE_WINDOW_SIZE)) {
    runtime_provider_storage_window_trap();
  }
  return window.current;
}

RuntimeProviderStorageWindow *RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_validate_tuple(
    RuntimeProviderStorageWindowView window) {
  RuntimeProviderStorageWindow *current =
      runtime_provider_storage_window_validate_allocated_tuple(window);
  if (current->signature != RUNTIME_PROVIDER_STORAGE_WINDOW_SIGNATURE ||
      current->initialized != 1 || current->backing == NULL) {
    runtime_provider_storage_window_trap();
  }
  return current;
}

static RuntimeProviderStorageWindow *
runtime_provider_storage_window_validate(RuntimeProviderStorageWindow *window) {
  if (window == NULL) {
    runtime_provider_storage_window_trap();
  }
  return runtime_provider_storage_window_validate_tuple(
      (RuntimeProviderStorageWindowView){
          .current = window,
          .lower_bound = window,
          .upper_bound = window + 1,
      });
}

static bool runtime_provider_storage_window_translate(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count,
    size_t *backing_offset,
    size_t *clipped_count
) {
  if ((byte_offset & (SIZE_MAX / 2 + 1)) != 0) {
    runtime_provider_storage_window_trap();
  }
  if (byte_offset >= window->byte_length ||
      byte_count > SIZE_MAX - byte_offset ||
      byte_offset > SIZE_MAX - window->backing_byte_offset) {
    return false;
  }
  const size_t remaining = window->byte_length - byte_offset;
  *backing_offset = window->backing_byte_offset + byte_offset;
  *clipped_count = byte_count > remaining ? remaining : byte_count;
  return true;
}

bool RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_slot0_forward(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
) {
  window = runtime_provider_storage_window_validate(window);
  size_t translated_offset = 0;
  size_t clipped_count = 0;
  if (!runtime_provider_storage_window_translate(window, byte_offset,
                                                 byte_count, &translated_offset,
                                                 &clipped_count)) {
    return false;
  }
  RuntimeProviderStorageBacking *backing = window->backing;
  if (backing->operations.slot0 == NULL) {
    runtime_provider_storage_window_trap();
  }
  return backing->operations.slot0(
      backing->context,
      translated_offset,
      clipped_count
  );
}

RuntimeProviderStorageTransfer RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_scale_transfer_by_element_shift(
    const RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
) {
  window = runtime_provider_storage_window_validate(
      (RuntimeProviderStorageWindow *)(uintptr_t)window);
  /* AArch64 variable 32-bit shifts use the low five shift bits. */
  const uint32_t shift = window->element_shift & UINT32_C(31);
  return (RuntimeProviderStorageTransfer){
      .byte_offset = (uint32_t)(element_offset << shift),
      .byte_count = (uint32_t)(element_count << shift),
  };
}

bool RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_slot1_forward(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
) {
  const RuntimeProviderStorageTransfer transfer =
      runtime_provider_storage_window_scale_transfer_by_element_shift(
          window,
          element_offset,
          element_count
      );
  return runtime_provider_storage_window_slot0_forward(
      window,
      transfer.byte_offset,
      transfer.byte_count
  );
}

bool RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_slot2_forward(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
) {
  window = runtime_provider_storage_window_validate(window);
  size_t translated_offset = 0;
  size_t clipped_count = 0;
  if (!runtime_provider_storage_window_translate(window, byte_offset,
                                                 byte_count, &translated_offset,
                                                 &clipped_count)) {
    return false;
  }
  RuntimeProviderStorageBacking *backing = window->backing;
  if (backing->operations.slot2 == NULL) {
    runtime_provider_storage_window_trap();
  }
  return backing->operations.slot2(
      backing->context,
      translated_offset,
      clipped_count
  );
}

bool RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_slot3_forward(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
) {
  const RuntimeProviderStorageTransfer transfer =
      runtime_provider_storage_window_scale_transfer_by_element_shift(
          window,
          element_offset,
          element_count
      );
  return runtime_provider_storage_window_slot2_forward(
      window,
      transfer.byte_offset,
      transfer.byte_count
  );
}

bool RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_slot4_forward(
    RuntimeProviderStorageWindow *window,
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    size_t byte_offset,
    size_t byte_count
) {
  window = runtime_provider_storage_window_validate(window);
  size_t translated_offset = 0;
  size_t clipped_count = 0;
  if (!runtime_provider_storage_window_translate(window, byte_offset,
                                                 byte_count, &translated_offset,
                                                 &clipped_count)) {
    return false;
  }
  RuntimeProviderStorageBacking *backing = window->backing;
  if (backing->operations.slot4 == NULL) {
    runtime_provider_storage_window_trap();
  }
  return backing->operations.slot4(
      backing->context,
      argument0,
      argument1,
      argument2,
      translated_offset,
      clipped_count
  );
}

void RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_initialize(
    RuntimeProviderStorageWindowView window,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
) {
  RuntimeProviderStorageWindow *current =
      runtime_provider_storage_window_validate_allocated_tuple(window);
  RuntimeProviderStorageBacking *backing_current =
      runtime_provider_storage_backing_validate(backing);
  if (backing_byte_offset > SIZE_MAX - byte_length ||
      backing_current->operations.slot0 == NULL ||
      backing_current->operations.slot2 == NULL ||
      backing_current->operations.slot4 == NULL) {
    runtime_provider_storage_window_trap();
  }

  *current = (RuntimeProviderStorageWindow){
      .signature = RUNTIME_PROVIDER_STORAGE_WINDOW_SIGNATURE,
      .initialized = 1,
      .element_shift = element_shift,
      .byte_length = byte_length,
      .slot0 = runtime_provider_storage_window_slot0_forward,
      .slot1 = runtime_provider_storage_window_slot1_forward,
      .slot2 = runtime_provider_storage_window_slot2_forward,
      .slot3 = runtime_provider_storage_window_slot3_forward,
      .slot4 = runtime_provider_storage_window_slot4_forward,
      .backing = backing_current,
      .backing_byte_offset = backing_byte_offset,
  };
}

RuntimeProviderStorageCallbackBinding RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_prepare_callback_binding(
    RuntimeProviderStorageWindowView window,
    const void *callback
) {
  (void)runtime_provider_storage_window_validate_tuple(window);
  if (callback == NULL) {
    runtime_provider_storage_window_trap();
  }
  return (RuntimeProviderStorageCallbackBinding){
      .object = window,
      .callback = callback,
  };
}

uint64_t RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_source_token(void) {
  return RUNTIME_PROVIDER_STORAGE_WINDOW_SOURCE_TOKEN;
}

RuntimeProviderStorageBackingView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_backing_descriptor_tuple(
    RuntimeProviderStorageBacking *backing) {
  if (backing == NULL) {
    runtime_provider_storage_window_trap();
  }
  return (RuntimeProviderStorageBackingView){
      .current = backing,
      .lower_bound = backing,
      .upper_bound = backing + 1,
  };
}

RuntimeProviderStorageBackingView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_save_secondary_tuple_x22_x19(
    RuntimeProviderStorageBackingView backing) {
  return backing;
}

RuntimeProviderStorageWindowView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_duplicate_current_lower(
    RuntimeProviderStorageWindowView window) {
  window.lower_bound = window.current;
  return window;
}

RuntimeProviderStorageBackingView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_restore_secondary_tuple_x22_x19(
    RuntimeProviderStorageBackingView backing) {
  return backing;
}

RuntimeProviderStorageWindowView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_create(
    RuntimeProviderStorageAllocate allocate,
    void *allocate_context,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
) {
  if (allocate == NULL) {
    runtime_provider_storage_window_trap();
  }
  RuntimeProviderStorageWindowView window =
      allocate(
          RUNTIME_PROVIDER_STORAGE_WINDOW_SIZE,
          _Alignof(RuntimeProviderStorageWindow),
          allocate_context
      );
  (void)runtime_provider_storage_window_validate_allocated_tuple(window);
  runtime_provider_storage_window_initialize(
      window,
      backing,
      backing_byte_offset,
      byte_length,
      element_shift
  );
  return window;
}

RuntimeProviderStorageBackingView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_storage_window_restore_saved_primary_to_secondary(
    RuntimeProviderStorageBackingView backing) {
  return backing;
}

RuntimeProviderStorageWindowView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_register_storage_window_dispatch_bridge(
    AVPRuntimeProvider *provider,
    uint64_t owner_domain,
    const char *lookup_key,
    const char *metadata_a,
    const char *metadata_b,
    RuntimeProviderStorageAllocate allocate,
    void *allocate_context,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
) {
  RuntimeProviderStorageWindowView window =
      runtime_provider_storage_window_create(
          allocate,
          allocate_context,
          backing,
          backing_byte_offset,
          byte_length,
          element_shift
      );
  runtime_provider_register(
      provider,
      owner_domain,
      lookup_key,
      metadata_a,
      metadata_b,
      window.current,
      (uintptr_t)window.lower_bound,
      (uintptr_t)window.upper_bound,
      runtime_provider_storage_window_source_token()
  );
  if (runtime_provider_publish(window.current) != 0) {
    runtime_provider_storage_window_trap();
  }
  return window;
}

RuntimeProviderStorageWindowView RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC
runtime_provider_register_storage_window(
    AVPRuntimeProvider *provider,
    uint64_t owner_domain,
    const char *lookup_key,
    const char *metadata_a,
    const char *metadata_b,
    RuntimeProviderStorageAllocate allocate,
    void *allocate_context,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
) {
  return runtime_provider_register_storage_window_dispatch_bridge(
      provider,
      owner_domain,
      lookup_key,
      metadata_a,
      metadata_b,
      allocate,
      allocate_context,
      backing,
      backing_byte_offset,
      byte_length,
      element_shift
  );
}

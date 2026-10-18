#pragma once

#include "avp/runtime/ipc/runtime_provider.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC __attribute__((used))
#endif

#define RUNTIME_PROVIDER_STORAGE_WINDOW_SIZE ((size_t)0x70)
#define RUNTIME_PROVIDER_STORAGE_BACKING_SIZE ((size_t)0x60)
#define RUNTIME_PROVIDER_STORAGE_WINDOW_SOURCE_TOKEN                           \
  UINT64_C(0x0dedfdebb84b0b42)

typedef struct RuntimeProviderStorageWindow RuntimeProviderStorageWindow;
typedef struct RuntimeProviderStorageBacking RuntimeProviderStorageBacking;

typedef bool (*RuntimeProviderStorageRawOperation)(
    void *context,
    size_t byte_offset,
    size_t byte_count
);
typedef bool (*RuntimeProviderStorageRawOperationWithContext)(
    void *context,
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    size_t byte_offset,
    size_t byte_count
);

typedef struct {
  RuntimeProviderStorageRawOperation slot0;
  RuntimeProviderStorageRawOperation slot2;
  RuntimeProviderStorageRawOperationWithContext slot4;
} RuntimeProviderStorageBackingOperations;

struct RuntimeProviderStorageBacking {
  void *context;
  RuntimeProviderStorageBackingOperations operations;
  uint8_t reserved_020[0x40];
};

typedef bool (*RuntimeProviderStorageWindowSlot0)(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
);
typedef bool (*RuntimeProviderStorageWindowSlot1)(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
);
typedef bool (*RuntimeProviderStorageWindowSlot2)(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
);
typedef bool (*RuntimeProviderStorageWindowSlot3)(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
);
typedef bool (*RuntimeProviderStorageWindowSlot4)(
    RuntimeProviderStorageWindow *window,
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    size_t byte_offset,
    size_t byte_count
);

struct RuntimeProviderStorageWindow {
  uint32_t signature;
  uint8_t initialized;
  uint8_t reserved_005[0x13];
  uint32_t element_shift;
  uint32_t reserved_01c;
  size_t byte_length;
  RuntimeProviderStorageWindowSlot0 slot0;
  RuntimeProviderStorageWindowSlot1 slot1;
  RuntimeProviderStorageWindowSlot2 slot2;
  RuntimeProviderStorageWindowSlot3 slot3;
  RuntimeProviderStorageWindowSlot4 slot4;
  uint8_t reserved_050[0x10];
  RuntimeProviderStorageBacking *backing;
  size_t backing_byte_offset;
};

_Static_assert(sizeof(RuntimeProviderStorageBacking) == 0x60,
               "storage backing descriptor size");
_Static_assert(offsetof(RuntimeProviderStorageWindow, element_shift) == 0x18,
               "storage window element shift offset");
_Static_assert(offsetof(RuntimeProviderStorageWindow, byte_length) == 0x20,
               "storage window length offset");
_Static_assert(offsetof(RuntimeProviderStorageWindow, backing) == 0x60,
               "storage window backing offset");
_Static_assert(offsetof(RuntimeProviderStorageWindow, backing_byte_offset) ==
                   0x68,
               "storage window backing byte offset");
_Static_assert(sizeof(RuntimeProviderStorageWindow) == 0x70,
               "storage window size");

typedef struct {
  RuntimeProviderStorageWindow *current;
  RuntimeProviderStorageWindow *lower_bound;
  RuntimeProviderStorageWindow *upper_bound;
} RuntimeProviderStorageWindowView;

typedef struct {
  RuntimeProviderStorageBacking *current;
  RuntimeProviderStorageBacking *lower_bound;
  RuntimeProviderStorageBacking *upper_bound;
} RuntimeProviderStorageBackingView;

typedef struct {
  size_t byte_offset;
  size_t byte_count;
} RuntimeProviderStorageTransfer;

typedef struct {
  RuntimeProviderStorageWindowView object;
  const void *callback;
} RuntimeProviderStorageCallbackBinding;

typedef RuntimeProviderStorageWindowView (*RuntimeProviderStorageAllocate)(
    size_t size,
    size_t alignment,
    void *context
);

RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindowView
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
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindowView
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
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC void
runtime_provider_storage_window_initialize(
    RuntimeProviderStorageWindowView window,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC bool
runtime_provider_storage_window_slot0_forward(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC bool
runtime_provider_storage_window_slot1_forward(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC bool
runtime_provider_storage_window_slot2_forward(
    RuntimeProviderStorageWindow *window,
    size_t byte_offset,
    size_t byte_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC bool
runtime_provider_storage_window_slot3_forward(
    RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC bool
runtime_provider_storage_window_slot4_forward(
    RuntimeProviderStorageWindow *window,
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    size_t byte_offset,
    size_t byte_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindow *
runtime_provider_storage_window_validate_tuple(
    RuntimeProviderStorageWindowView window);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageCallbackBinding
runtime_provider_storage_window_prepare_callback_binding(
    RuntimeProviderStorageWindowView window,
    const void *callback
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC uint64_t
runtime_provider_storage_window_source_token(void);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageBackingView
runtime_provider_storage_window_backing_descriptor_tuple(
    RuntimeProviderStorageBacking *backing);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageBackingView
runtime_provider_storage_window_save_secondary_tuple_x22_x19(
    RuntimeProviderStorageBackingView backing);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindowView
runtime_provider_storage_window_duplicate_current_lower(
    RuntimeProviderStorageWindowView window);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageBackingView
runtime_provider_storage_window_restore_secondary_tuple_x22_x19(
    RuntimeProviderStorageBackingView backing);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageTransfer
runtime_provider_storage_window_scale_transfer_by_element_shift(
    const RuntimeProviderStorageWindow *window,
    uint32_t element_offset,
    uint32_t element_count
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindowView
runtime_provider_storage_window_create(
    RuntimeProviderStorageAllocate allocate,
    void *allocate_context,
    RuntimeProviderStorageBackingView backing,
    size_t backing_byte_offset,
    size_t byte_length,
    uint32_t element_shift
);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageWindow *
runtime_provider_storage_window_validate_allocated_tuple(
    RuntimeProviderStorageWindowView window);
RUNTIME_PROVIDER_WINDOW_RECOVERED_PUBLIC RuntimeProviderStorageBackingView
runtime_provider_storage_window_restore_saved_primary_to_secondary(
    RuntimeProviderStorageBackingView backing);

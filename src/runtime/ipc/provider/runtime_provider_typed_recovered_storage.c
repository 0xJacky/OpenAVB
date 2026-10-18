#include "runtime_provider_typed_recovered_private.h"

#include <limits.h>

bool runtime_provider_normalize_storage_operation_record(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  storage = runtime_provider_typed_require_storage(storage);
  if (operation == 0 || operation->element_count == 0 ||
      storage->element_shift >= sizeof(size_t) * CHAR_BIT) {
    return false;
  }
  const size_t element_size = (size_t)1 << storage->element_shift;
  if (operation->element_offset > SIZE_MAX / element_size ||
      operation->element_count > SIZE_MAX / element_size) {
    return false;
  }
  operation->byte_offset = (size_t)operation->element_offset * element_size;
  operation->byte_count = (size_t)operation->element_count * element_size;
  if (operation->byte_offset > storage->byte_size ||
      operation->byte_count > storage->byte_size - operation->byte_offset ||
      operation->buffer == 0 ||
      operation->buffer_size < operation->byte_count) {
    operation->status = -1;
    operation->transferred = 0;
    return false;
  }
  operation->status = 0;
  operation->transferred = 0;
  return true;
}

bool runtime_provider_typed_storage_operation_dispatch(
    RuntimeProviderTypedStorage *storage,
    unsigned slot,
    RuntimeProviderStorageOperation *operation
) {
  storage = runtime_provider_typed_require_storage(storage);
  if (!runtime_provider_normalize_storage_operation_record(storage,
                                                           operation)) {
    return false;
  }
  return runtime_firebloom_descriptor_implementation_dispatch(
      storage->descriptor,
      slot,
      storage,
      operation
  );
}

bool runtime_provider_typed_storage_slot0_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_operation_dispatch(
      storage,
      0,
      operation
  );
}

bool runtime_provider_typed_storage_slot1_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_operation_dispatch(
      storage,
      1,
      operation
  );
}

bool runtime_provider_typed_storage_slot4_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_operation_dispatch(
      storage,
      4,
      operation
  );
}

static bool
runtime_provider_storage_copy(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation,
    bool write
) {
  storage = runtime_provider_typed_require_storage(storage);
  if (operation == 0 || operation->byte_offset > storage->byte_size ||
      operation->byte_count > storage->byte_size - operation->byte_offset ||
      operation->buffer == 0 ||
      operation->buffer_size < operation->byte_count) {
    return false;
  }
  uint8_t *storage_bytes = storage->bytes + operation->byte_offset;
  for (size_t index = 0; index < operation->byte_count; ++index) {
    if (write) {
      storage_bytes[index] = operation->buffer[index];
    } else {
      operation->buffer[index] = storage_bytes[index];
    }
  }
  operation->transferred = operation->byte_count;
  operation->status = 0;
  return true;
}

bool runtime_provider_typed_storage_read_route(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  storage = runtime_provider_typed_require_storage(storage);
  RuntimeProviderRouteValue route = storage->implementation->read_route;
  if (route.call != 0) {
    return route.call(route.context, storage, operation);
  }
  return runtime_provider_storage_copy(storage, operation, false);
}

bool runtime_provider_typed_storage_write_route(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  storage = runtime_provider_typed_require_storage(storage);
  RuntimeProviderRouteValue route = storage->implementation->write_route;
  if (route.call != 0) {
    return route.call(route.context, storage, operation);
  }
  return runtime_provider_storage_copy(storage, operation, true);
}

bool runtime_provider_typed_storage_read_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_read_route(storage, operation);
}

bool runtime_provider_typed_storage_write_dispatch_bridge(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_write_route(storage, operation);
}

bool runtime_provider_typed_storage_read_route_thunk(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_read_route(storage, operation);
}

bool runtime_provider_typed_storage_read(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    void *destination,
    size_t byte_count
) {
  RuntimeProviderStorageOperation operation = {
      .kind = RUNTIME_PROVIDER_OPERATION_READ,
      .buffer = destination,
      .buffer_size = byte_count,
      .byte_offset = byte_offset,
      .byte_count = byte_count,
  };
  return runtime_provider_typed_storage_read_route(storage, &operation) &&
         operation.transferred == byte_count;
}

bool runtime_provider_typed_storage_write(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    const void *source,
    size_t byte_count
) {
  RuntimeProviderStorageOperation operation = {
      .kind = RUNTIME_PROVIDER_OPERATION_WRITE,
      .buffer = (uint8_t *)(uintptr_t)source,
      .buffer_size = byte_count,
      .byte_offset = byte_offset,
      .byte_count = byte_count,
  };
  return runtime_provider_typed_storage_write_route(storage, &operation) &&
         operation.transferred == byte_count;
}

bool runtime_provider_typed_storage_compare(
    RuntimeProviderTypedStorage *storage,
    size_t byte_offset,
    const void *expected,
    size_t byte_count
) {
  storage = runtime_provider_typed_require_storage(storage);
  if (expected == 0 || byte_offset > storage->byte_size ||
      byte_count > storage->byte_size - byte_offset) {
    return false;
  }
  const uint8_t *right = expected;
  for (size_t index = 0; index < byte_count; ++index) {
    if (storage->bytes[byte_offset + index] != right[index]) {
      return false;
    }
  }
  return true;
}

bool runtime_provider_typed_storage_slot0_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_slot0_dispatch_bridge(
      storage,
      operation
  );
}

bool runtime_provider_typed_storage_slot1_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_slot1_dispatch_bridge(
      storage,
      operation
  );
}

bool runtime_provider_typed_storage_slot4_invoke(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  return runtime_provider_typed_storage_slot4_dispatch_bridge(
      storage,
      operation
  );
}

static bool runtime_provider_typed_storage_unsupported(
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  (void)runtime_provider_typed_require_storage(storage);
  if (operation != 0) {
    operation->status = -1;
    operation->transferred = 0;
  }
  return false;
}

#define RUNTIME_PROVIDER_DEFINE_UNSUPPORTED(name)                              \
  bool name(RuntimeProviderTypedStorage *storage,                              \
            RuntimeProviderStorageOperation *operation) {                      \
    return runtime_provider_typed_storage_unsupported(storage, operation);     \
  }
RUNTIME_PROVIDER_DEFINE_UNSUPPORTED(
    runtime_provider_typed_storage_slot1_unsupported)
RUNTIME_PROVIDER_DEFINE_UNSUPPORTED(
    runtime_provider_typed_storage_slot3_unsupported)
RUNTIME_PROVIDER_DEFINE_UNSUPPORTED(
    runtime_provider_typed_storage_slot4_unsupported)
RUNTIME_PROVIDER_DEFINE_UNSUPPORTED(
    runtime_provider_typed_storage_slot5_unsupported)
#undef RUNTIME_PROVIDER_DEFINE_UNSUPPORTED

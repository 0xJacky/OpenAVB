#include "runtime_provider_typed_recovered_private.h"

_Noreturn void runtime_provider_typed_fail(void) { __builtin_trap(); }

uint32_t runtime_provider_alignment_shift(size_t alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    runtime_provider_typed_fail();
  }
  uint32_t shift = 0;
  while (((size_t)1 << shift) != alignment) {
    ++shift;
  }
  return shift;
}

static void runtime_provider_descriptor_set_alignment(
    RuntimeProviderTypedDescriptor *descriptor,
    size_t alignment,
    bool above_instance_size
) {
  if (descriptor == 0 || descriptor->instance_size == 0 ||
      (above_instance_size && alignment <= descriptor->instance_size) ||
      (!above_instance_size && alignment > descriptor->instance_size)) {
    runtime_provider_typed_fail();
  }
  descriptor->alignment_shift = runtime_provider_alignment_shift(alignment);
  descriptor->alignment = alignment;
}

void runtime_provider_set_descriptor_alignment_within_instance_size(
    RuntimeProviderTypedDescriptor *descriptor,
    size_t alignment
) {
  runtime_provider_descriptor_set_alignment(descriptor, alignment, false);
}

void runtime_provider_set_descriptor_alignment_above_instance_size(
    RuntimeProviderTypedDescriptor *descriptor,
    size_t alignment
) {
  runtime_provider_descriptor_set_alignment(descriptor, alignment, true);
}

RuntimeProviderTypedImplementation *
runtime_provider_typed_require_implementation(
    RuntimeProviderTypedDescriptor *descriptor) {
  if (descriptor == 0 || !descriptor->enabled ||
      descriptor->implementation == 0 || !descriptor->implementation->enabled ||
      descriptor->implementation->descriptor != descriptor) {
    runtime_provider_typed_fail();
  }
  return descriptor->implementation;
}

RuntimeProviderTypedStorage *
runtime_provider_typed_require_storage(RuntimeProviderTypedStorage *storage) {
  if (storage == 0 || storage->bytes == 0 || storage->byte_size == 0 ||
      storage->descriptor == 0 || storage->implementation == 0 ||
      storage->descriptor->implementation != storage->implementation ||
      !storage->descriptor->enabled || !storage->implementation->enabled) {
    runtime_provider_typed_fail();
  }
  return storage;
}

RuntimeProviderTypedImplementation *runtime_provider_create_implementation(
    RuntimeProviderTypedDescriptor *descriptor,
    RuntimeProviderTypedImplementation *implementation,
    void *context
) {
  if (descriptor == 0 || implementation == 0 ||
      descriptor->instance_size == 0 || descriptor->implementation != 0) {
    runtime_provider_typed_fail();
  }
  if (descriptor->alignment == 0) {
    descriptor->alignment = 1;
    descriptor->alignment_shift = 0;
  }
  *implementation = (RuntimeProviderTypedImplementation){
      .descriptor = descriptor,
      .context = context,
      .alignment = descriptor->alignment,
      .alignment_shift = descriptor->alignment_shift,
      .enabled = true,
  };
  descriptor->implementation = implementation;
  descriptor->enabled = true;
  return implementation;
}

bool runtime_provider_implementation_is_enabled(
    const RuntimeProviderTypedImplementation *implementation) {
  return implementation != 0 && implementation->enabled;
}

bool runtime_firebloom_descriptor_implementation_is_enabled(
    const RuntimeProviderTypedDescriptor *descriptor) {
  return descriptor != 0 && descriptor->enabled &&
         runtime_provider_implementation_is_enabled(descriptor->implementation);
}

RuntimeProviderTypedImplementation *
runtime_provider_resolve_implementation_dispatch_tuple(
    RuntimeProviderTypedDescriptor *descriptor) {
  return runtime_provider_typed_require_implementation(descriptor);
}

RuntimeProviderTypedImplementation *
runtime_firebloom_descriptor_retain_implementation(
    RuntimeProviderTypedDescriptor *descriptor) {
  RuntimeProviderTypedImplementation *implementation =
      runtime_provider_typed_require_implementation(descriptor);
  if (implementation->references == UINT32_MAX) {
    runtime_provider_typed_fail();
  }
  ++implementation->references;
  return implementation;
}

static void runtime_provider_bind_callback(
    RuntimeProviderTypedImplementation *implementation,
    unsigned slot,
    RuntimeProviderTypedCallback callback
) {
  if (!runtime_provider_implementation_is_enabled(implementation) ||
      slot >= RUNTIME_PROVIDER_TYPED_SLOT_COUNT || callback == 0) {
    runtime_provider_typed_fail();
  }
  implementation->callbacks[slot] = callback;
}

#define RUNTIME_PROVIDER_DEFINE_BIND_SLOT(name, number)                        \
  void name(RuntimeProviderTypedImplementation *implementation,                \
            RuntimeProviderTypedCallback callback) {                           \
    runtime_provider_bind_callback(implementation, number, callback);          \
  }
RUNTIME_PROVIDER_DEFINE_BIND_SLOT(runtime_provider_bind_callback_slot0, 0)
RUNTIME_PROVIDER_DEFINE_BIND_SLOT(runtime_provider_bind_callback_slot1, 1)
RUNTIME_PROVIDER_DEFINE_BIND_SLOT(runtime_provider_bind_callback_slot2, 2)
RUNTIME_PROVIDER_DEFINE_BIND_SLOT(runtime_provider_bind_callback_slot3, 3)
RUNTIME_PROVIDER_DEFINE_BIND_SLOT(runtime_provider_bind_callback_slot4, 4)
#undef RUNTIME_PROVIDER_DEFINE_BIND_SLOT

static void
runtime_provider_bind_route(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue *destination,
    RuntimeProviderRouteValue route
) {
  if (!runtime_provider_implementation_is_enabled(implementation) ||
      destination == 0 || route.call == 0) {
    runtime_provider_typed_fail();
  }
  *destination = route;
}

void runtime_provider_bind_read_route(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route
) {
  runtime_provider_bind_route(
      implementation,
      &implementation->read_route,
      route
  );
}

void runtime_provider_bind_write_route(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route
) {
  runtime_provider_bind_route(
      implementation,
      &implementation->write_route,
      route
  );
}

void runtime_provider_bind_read_route_dispatch_bridge(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route
) {
  runtime_provider_bind_read_route(implementation, route);
}

void runtime_provider_bind_write_route_dispatch_bridge(
    RuntimeProviderTypedImplementation *implementation,
    RuntimeProviderRouteValue route
) {
  runtime_provider_bind_write_route(implementation, route);
}

static void runtime_provider_implementation_set_alignment(
    RuntimeProviderTypedImplementation *implementation,
    size_t alignment,
    bool above_instance_size
) {
  if (!runtime_provider_implementation_is_enabled(implementation)) {
    runtime_provider_typed_fail();
  }
  const size_t instance_size = implementation->descriptor->instance_size;
  if ((above_instance_size && alignment <= instance_size) ||
      (!above_instance_size && alignment > instance_size)) {
    runtime_provider_typed_fail();
  }
  implementation->alignment_shift = runtime_provider_alignment_shift(alignment);
  implementation->alignment = alignment;
}

void runtime_provider_set_implementation_alignment_within_instance_size(
    RuntimeProviderTypedImplementation *implementation,
    size_t alignment
) {
  runtime_provider_implementation_set_alignment(
      implementation,
      alignment,
      false
  );
}

void runtime_provider_set_implementation_alignment_above_instance_size(
    RuntimeProviderTypedImplementation *implementation,
    size_t alignment
) {
  runtime_provider_implementation_set_alignment(
      implementation,
      alignment,
      true
  );
}

bool runtime_firebloom_descriptor_implementation_dispatch(
    RuntimeProviderTypedDescriptor *descriptor,
    unsigned slot,
    RuntimeProviderTypedStorage *storage,
    RuntimeProviderStorageOperation *operation
) {
  RuntimeProviderTypedImplementation *implementation =
      runtime_provider_typed_require_implementation(descriptor);
  if (slot >= RUNTIME_PROVIDER_TYPED_SLOT_COUNT ||
      implementation->callbacks[slot] == 0) {
    return false;
  }
  return implementation->callbacks[slot](implementation, storage, operation);
}

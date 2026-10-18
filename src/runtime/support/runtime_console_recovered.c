#include "avp/runtime/support/runtime_console_recovered.h"

enum {
  RUNTIME_CONSOLE_INPUT_CAPACITY = 0x100,
  RUNTIME_CONSOLE_WRITE_CHUNK_SIZE = 0x2000,
  RUNTIME_CONSOLE_LOCK_OFFSET = 0x68,
  RUNTIME_CONSOLE_LOCK_UPPER_OFFSET = 0x90,
  RUNTIME_CONSOLE_EVENT_FLAGS = 0x0b,
};

const RuntimeFirebloomType runtime_console_lock_type = {
    .identity = UINT64_C(0x636f6e736c6f636b),
    .element_size =
        RUNTIME_CONSOLE_LOCK_UPPER_OFFSET - RUNTIME_CONSOLE_LOCK_OFFSET,
    .bytewise_copyable = false,
};

static RuntimeConsoleConfiguration g_runtime_console_configuration;
static uint16_t g_runtime_console_input_event;

static void runtime_console_trap(void) { __builtin_trap(); }

static const uint8_t *runtime_console_validate_span(
    RuntimeFirebloomSpan source,
    size_t size
) {
  const uint8_t *bytes = runtime_byte_ring_validate_byte_storage(source, size);

  if (bytes == NULL) {
    runtime_console_trap();
  }
  return bytes;
}

static RuntimeFirebloomSpan
runtime_console_advance_span(RuntimeFirebloomSpan source, size_t count) {
  source.current = (uint8_t *)source.current + count;
  return source;
}

static void runtime_console_write_chunk(
    RuntimeFirebloomSpan source,
    size_t size
) {
  switch (g_runtime_console_configuration.route) {
  case RUNTIME_CONSOLE_ROUTE_BUFFERED:
    runtime_console_output_buffered_write(source, size, false);
    return;
  case RUNTIME_CONSOLE_ROUTE_ROOT_RPC:
    if (g_runtime_console_configuration.root_write(
            runtime_console_validate_span(source, size), size,
            g_runtime_console_configuration.context) != size) {
      runtime_console_trap();
    }
    return;
  case RUNTIME_CONSOLE_ROUTE_DISCARD:
    runtime_console_discard_span(source, size);
    return;
  }
  runtime_console_trap();
}

void runtime_console_configure(
    const RuntimeConsoleConfiguration *configuration) {
  if (configuration == NULL || configuration->event_create == NULL ||
      configuration->event_set == NULL || configuration->event_clear == NULL ||
      configuration->yield == NULL || configuration->lock == NULL ||
      configuration->unlock == NULL || configuration->root_write == NULL) {
    runtime_console_trap();
  }

  g_runtime_console_configuration.input_control = configuration->input_control;
  g_runtime_console_configuration.input_storage = configuration->input_storage;
  g_runtime_console_configuration.input_physical_region =
      configuration->input_physical_region;
  g_runtime_console_configuration.output_control =
      configuration->output_control;
  g_runtime_console_configuration.output_drain_event =
      configuration->output_drain_event;
  g_runtime_console_configuration.route = configuration->route;
  g_runtime_console_configuration.event_create = configuration->event_create;
  g_runtime_console_configuration.event_set = configuration->event_set;
  g_runtime_console_configuration.event_clear = configuration->event_clear;
  g_runtime_console_configuration.yield = configuration->yield;
  g_runtime_console_configuration.lock = configuration->lock;
  g_runtime_console_configuration.unlock = configuration->unlock;
  g_runtime_console_configuration.root_write = configuration->root_write;
  g_runtime_console_configuration.context = configuration->context;
  g_runtime_console_input_event = 0;
}

void runtime_console_input_initialize(void) {
  g_runtime_console_input_event = g_runtime_console_configuration.event_create(
      RUNTIME_CONSOLE_EVENT_FLAGS,
      g_runtime_console_configuration.context
  );
  if (g_runtime_console_input_event == 0) {
    runtime_console_trap();
  }

  (void)runtime_byte_ring_initialize(
      g_runtime_console_configuration.input_control,
      g_runtime_console_configuration.input_storage,
      g_runtime_console_configuration.input_physical_region,
      RUNTIME_CONSOLE_INPUT_CAPACITY,
      runtime_console_input_ring_push_notify,
      runtime_console_input_ring_pop_notify,
      g_runtime_console_configuration.context,
      0
  );
}

void runtime_console_input_ring_push_notify(void *context) {
  (void)context;
  if (g_runtime_console_input_event == 0) {
    runtime_console_trap();
  }
  g_runtime_console_configuration.event_set(
      g_runtime_console_input_event,
      g_runtime_console_configuration.context
  );
}

void runtime_console_input_ring_pop_notify(void *context) {
  (void)context;
  if (g_runtime_console_input_event == 0) {
    runtime_console_trap();
  }
  g_runtime_console_configuration.event_clear(
      g_runtime_console_input_event,
      g_runtime_console_configuration.context
  );
}

void runtime_console_output_buffered_write(
    RuntimeFirebloomSpan source,
    size_t size,
    bool raw
) {
  static const uint8_t crlf_bytes[2] = {'\r', '\n'};
  static const RuntimeFirebloomType byte_type = {
      .identity = UINT64_C(0x62797465),
      .element_size = 1,
      .bytewise_copyable = true,
  };
  const RuntimeFirebloomSpan crlf = {
      .current = (void *)crlf_bytes,
      .lower_bound = (void *)crlf_bytes,
      .upper_bound = (void *)(crlf_bytes + sizeof(crlf_bytes)),
      .type = &byte_type,
  };
  const uint8_t *bytes = runtime_console_validate_span(source, size);

  g_runtime_console_configuration.lock(g_runtime_console_configuration.context);
  if (raw) {
    runtime_console_output_ring_write_all(source, size);
  } else {
    size_t segment_start = 0;

    for (size_t index = 0; index < size; ++index) {
      if (bytes[index] != '\n') {
        continue;
      }
      if (index != segment_start) {
        runtime_console_output_ring_write_all(
            runtime_console_advance_span(source, segment_start),
            index - segment_start
        );
      }
      runtime_console_output_ring_write_all(crlf, sizeof(crlf_bytes));
      segment_start = index + 1;
    }
    if (segment_start != size) {
      runtime_console_output_ring_write_all(
          runtime_console_advance_span(source, segment_start),
          size - segment_start
      );
    }
  }
  g_runtime_console_configuration.unlock(
      g_runtime_console_configuration.context);
  runtime_console_output_drain_signal();
}

void runtime_console_output_ring_write_all(
    RuntimeFirebloomSpan source,
    size_t size
) {
  runtime_console_validate_span(source, size);
  while (size != 0) {
    const size_t written = runtime_byte_ring_write(
        g_runtime_console_configuration.output_control,
        source,
        size
    );

    source = runtime_console_advance_span(source, written);
    size -= written;
    if (size == 0) {
      return;
    }

    /* The writer drops its recursive lock while the ring drains. */
    g_runtime_console_configuration.unlock(
        g_runtime_console_configuration.context);
    runtime_console_output_drain_signal();
    g_runtime_console_configuration.yield(
        g_runtime_console_configuration.context);
    g_runtime_console_configuration.lock(
        g_runtime_console_configuration.context);
  }
}

void runtime_console_output_drain_signal(void) {
  if (g_runtime_console_configuration.output_drain_event == 0) {
    runtime_console_trap();
  }
  g_runtime_console_configuration.event_set(
      g_runtime_console_configuration.output_drain_event,
      g_runtime_console_configuration.context
  );
}

void runtime_console_discard_span(RuntimeFirebloomSpan source, size_t size) {
  (void)runtime_console_validate_span(source, size);
}

void runtime_console_write_cstring(RuntimeFirebloomSpan source) {
  const uint8_t *bytes;
  const uintptr_t current = (uintptr_t)source.current;
  const uintptr_t upper = (uintptr_t)source.upper_bound;
  size_t size = 0;

  if (current >= upper) {
    runtime_console_trap();
  }
  bytes = runtime_console_validate_span(source, upper - current);
  while (size < upper - current && bytes[size] != 0) {
    ++size;
  }
  if (size == upper - current) {
    runtime_console_trap();
  }
  runtime_console_write_span(source, size);
}

void runtime_console_write_span(RuntimeFirebloomSpan source, size_t size) {
  runtime_console_validate_span(source, size);
  while (size != 0) {
    const size_t chunk = size < RUNTIME_CONSOLE_WRITE_CHUNK_SIZE
                             ? size
                             : RUNTIME_CONSOLE_WRITE_CHUNK_SIZE;

    runtime_console_write_chunk(source, chunk);
    source = runtime_console_advance_span(source, chunk);
    size -= chunk;
  }
}

RuntimeFirebloomSpan
runtime_console_outlined_restore_span_x22_x19(RuntimeFirebloomSpan saved) {
  return saved;
}

RuntimeFirebloomSpan
runtime_console_outlined_save_span_x22_x19(RuntimeFirebloomSpan incoming) {
  return incoming;
}

RuntimeFirebloomSpan runtime_console_outlined_restore_span_bounds_x21_x19(
    void *current,
    RuntimeFirebloomSpan saved
) {
  saved.current = current;
  return saved;
}

RuntimeFirebloomSpan runtime_console_outlined_lock_tuple_x26(void *state) {
  return runtime_console_outlined_lock_tuple_x25(
      state,
      &runtime_console_lock_type
  );
}

RuntimeFirebloomSpan
runtime_console_outlined_lock_tuple_x25(
    void *state,
    const RuntimeFirebloomType *type
) {
  uint8_t *base = state;

  if (base == NULL || type == NULL) {
    runtime_console_trap();
  }
  return (RuntimeFirebloomSpan){
      .current = base + RUNTIME_CONSOLE_LOCK_OFFSET,
      .lower_bound = base + RUNTIME_CONSOLE_LOCK_OFFSET,
      .upper_bound = base + RUNTIME_CONSOLE_LOCK_UPPER_OFFSET,
      .type = type,
  };
}

int32_t runtime_console_write_cstring_status(RuntimeFirebloomSpan source) {
  runtime_console_write_cstring(source);
  return 0;
}

#pragma once

#include "avp/runtime/support/runtime_byte_ring.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_CONSOLE_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_CONSOLE_RECOVERED_ENTRY __attribute__((used))
#endif

typedef enum {
  RUNTIME_CONSOLE_ROUTE_BUFFERED = 0,
  RUNTIME_CONSOLE_ROUTE_ROOT_RPC = 1,
  RUNTIME_CONSOLE_ROUTE_DISCARD = 2,
} RuntimeConsoleRoute;

typedef uint16_t (*RuntimeConsoleEventCreate)(uint32_t flags, void *context);
typedef void (*RuntimeConsoleEventUpdate)(uint16_t handle, void *context);
typedef void (*RuntimeConsoleOperation)(void *context);
typedef size_t (*RuntimeConsoleRootWrite)(
  const uint8_t *bytes,
  size_t size,
  void *context
);

typedef struct {
  RuntimeByteRingControlTuple input_control;
  RuntimeFirebloomSpan input_storage;
  RuntimeByteRingPhysicalRegion input_physical_region;
  RuntimeByteRingControlTuple output_control;
  uint16_t output_drain_event;
  RuntimeConsoleRoute route;
  RuntimeConsoleEventCreate event_create;
  RuntimeConsoleEventUpdate event_set;
  RuntimeConsoleEventUpdate event_clear;
  RuntimeConsoleOperation yield;
  RuntimeConsoleOperation lock;
  RuntimeConsoleOperation unlock;
  RuntimeConsoleRootWrite root_write;
  void *context;
} RuntimeConsoleConfiguration;

extern const RuntimeFirebloomType runtime_console_lock_type;

void runtime_console_configure(
    const RuntimeConsoleConfiguration *configuration);

RUNTIME_CONSOLE_RECOVERED_ENTRY void runtime_console_input_initialize(void);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_input_ring_push_notify(void *context);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_input_ring_pop_notify(void *context);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_output_buffered_write(
  RuntimeFirebloomSpan source,
  size_t size,
  bool raw
);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_output_ring_write_all(RuntimeFirebloomSpan source, size_t size);
RUNTIME_CONSOLE_RECOVERED_ENTRY void runtime_console_output_drain_signal(void);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_discard_span(RuntimeFirebloomSpan source, size_t size);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_write_cstring(RuntimeFirebloomSpan source);
RUNTIME_CONSOLE_RECOVERED_ENTRY void
runtime_console_write_span(RuntimeFirebloomSpan source, size_t size);
RUNTIME_CONSOLE_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_console_outlined_restore_span_x22_x19(RuntimeFirebloomSpan saved);
RUNTIME_CONSOLE_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_console_outlined_save_span_x22_x19(RuntimeFirebloomSpan incoming);
RUNTIME_CONSOLE_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_console_outlined_restore_span_bounds_x21_x19(
  void *current,
  RuntimeFirebloomSpan saved
);
RUNTIME_CONSOLE_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_console_outlined_lock_tuple_x26(void *state);
RUNTIME_CONSOLE_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_console_outlined_lock_tuple_x25(
  void *state,
  const RuntimeFirebloomType *type
);
RUNTIME_CONSOLE_RECOVERED_ENTRY int32_t
runtime_console_write_cstring_status(RuntimeFirebloomSpan source);

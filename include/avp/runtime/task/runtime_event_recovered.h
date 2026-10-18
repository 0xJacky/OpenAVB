#pragma once

#include "avp/runtime/task/runtime_event.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_EVENT_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_EVENT_RECOVERED_ENTRY __attribute__((used))
#endif

enum {
  RUNTIME_EVENT_REGISTRATION_TYPE_TOKEN = 0x65767267,
};

typedef struct {
  RuntimeEvent *current;
  RuntimeEvent *lower_bound;
  RuntimeEvent *upper_bound;
} RuntimeEventView;

typedef struct {
  uint16_t *current;
  uint16_t *lower_bound;
  uint16_t *upper_bound;
  uintptr_t cookie;
  uint8_t initially_signaled;
  uint8_t auto_reset;
  uint8_t reserved_22[6];
} RuntimeEventStaticDescriptor;

typedef struct {
  RuntimeEventStaticDescriptor *current;
  RuntimeEventStaticDescriptor *lower_bound;
  RuntimeEventStaticDescriptor *upper_bound;
  uintptr_t type_token;
} RuntimeEventRegistrationTuple;

_Static_assert(sizeof(RuntimeEventStaticDescriptor) == 0x28,
               "static event descriptor size");
_Static_assert(offsetof(RuntimeEventStaticDescriptor, initially_signaled) ==
                   0x20,
               "static event initial state offset");
_Static_assert(offsetof(RuntimeEventStaticDescriptor, auto_reset) == 0x21,
               "static event mode offset");
_Static_assert(sizeof(RuntimeEventRegistrationTuple) == 0x20,
               "event registration tuple size");

typedef uint16_t (*RuntimeEventRecoveredCreate)(
  bool auto_reset,
  bool initially_signaled,
  void *context
);
typedef bool (*RuntimeEventRecoveredWait)(
  uint16_t handle,
  uint64_t absolute_deadline,
  void *context
);
typedef void (*RuntimeEventRecoveredSet)(uint16_t handle, void *context);
typedef uint64_t (*RuntimeEventRecoveredClockRead)(void *context);
typedef uint64_t (*RuntimeEventRecoveredDurationConvert)(
  uint64_t microseconds,
  void *context
);
typedef RuntimeEventView (*RuntimeEventRecoveredResolve)(
  uint64_t object_id,
  size_t object_size,
  void *context
);
typedef bool (*RuntimeEventRecoveredWakeOne)(
  RuntimeEvent *event,
  void *context
);

typedef struct {
  const RuntimeEventRegistrationTuple *registrations;
  size_t registration_count;
  uint64_t *initialization_timestamp;
  RuntimeEventRecoveredCreate create;
  RuntimeEventRecoveredWait wait;
  RuntimeEventRecoveredSet set;
  RuntimeEventRecoveredClockRead counter_read;
  RuntimeEventRecoveredClockRead microseconds_read;
  RuntimeEventRecoveredDurationConvert microseconds_to_ticks;
  RuntimeEventRecoveredResolve resolve;
  RuntimeEventRecoveredWakeOne wake_one;
  void *context;
} RuntimeEventRecoveredConfiguration;

void runtime_event_recovered_configure(
    const RuntimeEventRecoveredConfiguration *configuration);

RUNTIME_EVENT_RECOVERED_ENTRY uint16_t
runtime_event_create_svc(bool auto_reset, bool initially_signaled);
RUNTIME_EVENT_RECOVERED_ENTRY void runtime_event_wait_forever(uint16_t handle);
RUNTIME_EVENT_RECOVERED_ENTRY bool
runtime_event_wait_with_timeout(uint16_t handle, uint64_t timeout_microseconds);
RUNTIME_EVENT_RECOVERED_ENTRY bool
runtime_event_set_timer_callback(uint64_t handle);
RUNTIME_EVENT_RECOVERED_ENTRY _Noreturn void
runtime_event_static_initialization_duplicate_fatal(void);
RUNTIME_EVENT_RECOVERED_ENTRY bool runtime_event_blocking_wait_is_enabled(void);
RUNTIME_EVENT_RECOVERED_ENTRY RuntimeEventView
runtime_event_resolve_object_id(uint64_t object_id);
RUNTIME_EVENT_RECOVERED_ENTRY void
runtime_event_assign_signal_state(RuntimeEventView event, bool signaled);
RUNTIME_EVENT_RECOVERED_ENTRY void
runtime_event_initialize_static_records(void);
RUNTIME_EVENT_RECOVERED_ENTRY RuntimeEventStaticDescriptor *
runtime_event_registration_tuple_validate(RuntimeEventRegistrationTuple tuple);

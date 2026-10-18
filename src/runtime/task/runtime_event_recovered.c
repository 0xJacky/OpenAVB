#include "avp/runtime/task/runtime_event_recovered.h"

#include "avp/runtime/support/runtime_time.h"

extern bool runtime_event_wake_one(RuntimeEvent *event);

static RuntimeEventRecoveredConfiguration event_configuration;
static bool event_configuration_bound;
static bool blocking_wait_enabled;

static _Noreturn void runtime_event_recovered_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static bool runtime_event_descriptor_in_bounds(
    const RuntimeEventStaticDescriptor *current,
    const RuntimeEventStaticDescriptor *lower_bound,
    const RuntimeEventStaticDescriptor *upper_bound
) {
  const uintptr_t address = (uintptr_t)current;
  const uintptr_t lower = (uintptr_t)lower_bound;
  const uintptr_t upper = (uintptr_t)upper_bound;

  return address != 0 && lower != 0 && upper != 0 && address >= lower &&
         address < upper;
}

static bool runtime_event_object_in_bounds(const RuntimeEventView event) {
  const uintptr_t address = (uintptr_t)event.current;
  const uintptr_t lower = (uintptr_t)event.lower_bound;
  const uintptr_t upper = (uintptr_t)event.upper_bound;

  return address != 0 && lower != 0 && upper != 0 && address >= lower &&
         address < upper;
}

static uint16_t runtime_event_create_operation(
    bool auto_reset,
    bool initially_signaled
) {
  if (event_configuration_bound && event_configuration.create != NULL) {
    return event_configuration.create(
        auto_reset,
        initially_signaled,
        event_configuration.context
    );
  }
  return runtime_event_create(auto_reset, initially_signaled);
}

static bool runtime_event_wait_operation(
    uint16_t handle,
    uint64_t absolute_deadline
) {
  if (event_configuration_bound && event_configuration.wait != NULL) {
    return event_configuration.wait(
        handle,
        absolute_deadline,
        event_configuration.context
    );
  }
  return runtime_event_wait(handle, absolute_deadline);
}

static void runtime_event_set_operation(uint16_t handle) {
  if (event_configuration_bound && event_configuration.set != NULL) {
    event_configuration.set(handle, event_configuration.context);
    return;
  }
  runtime_event_set(handle);
}

static uint64_t runtime_event_counter_read(void) {
  if (event_configuration_bound && event_configuration.counter_read != NULL) {
    return event_configuration.counter_read(event_configuration.context);
  }
  return avp_virtual_counter_read();
}

static uint64_t runtime_event_microseconds_read(void) {
  if (event_configuration_bound &&
      event_configuration.microseconds_read != NULL) {
    return event_configuration.microseconds_read(event_configuration.context);
  }
  return runtime_microsecond_time_read();
}

static uint64_t runtime_event_microseconds_to_ticks(uint64_t microseconds) {
  if (event_configuration_bound &&
      event_configuration.microseconds_to_ticks != NULL) {
    return event_configuration.microseconds_to_ticks(
        microseconds,
        event_configuration.context
    );
  }
  return avp_microseconds_to_ticks(microseconds);
}

static bool runtime_event_wake_one_operation(RuntimeEvent *event) {
  if (event_configuration_bound && event_configuration.wake_one != NULL) {
    return event_configuration.wake_one(event, event_configuration.context);
  }
  return runtime_event_wake_one(event);
}

void runtime_event_recovered_configure(
    const RuntimeEventRecoveredConfiguration *configuration) {
  if (configuration == NULL || (configuration->registration_count != 0 &&
                                configuration->registrations == NULL)) {
    runtime_event_recovered_trap();
  }
  event_configuration = *configuration;
  event_configuration_bound = true;
  blocking_wait_enabled = false;
}

uint16_t RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_create_svc(bool auto_reset, bool initially_signaled) {
  return runtime_event_create_operation(auto_reset, initially_signaled);
}

bool RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_blocking_wait_is_enabled(void) {
  return blocking_wait_enabled;
}

bool RUNTIME_EVENT_RECOVERED_ENTRY runtime_event_wait_with_timeout(
    uint16_t handle,
    uint64_t timeout_microseconds
) {
  if (runtime_event_blocking_wait_is_enabled()) {
    const uint64_t deadline =
        timeout_microseconds == UINT64_MAX
            ? UINT64_MAX
            : runtime_event_counter_read() +
                  runtime_event_microseconds_to_ticks(timeout_microseconds);
    return runtime_event_wait_operation(handle, deadline);
  }

  uint64_t deadline = timeout_microseconds;
  if (timeout_microseconds != 0 && timeout_microseconds != UINT64_MAX) {
    deadline = runtime_event_microseconds_read() + timeout_microseconds;
  }
  do {
    if (runtime_event_wait_operation(handle, 0)) {
      return true;
    }
  } while (runtime_event_microseconds_read() <= deadline);
  return false;
}

void RUNTIME_EVENT_RECOVERED_ENTRY runtime_event_wait_forever(uint16_t handle) {
  if (!runtime_event_wait_with_timeout(handle, UINT64_MAX)) {
    runtime_event_recovered_trap();
  }
}

bool RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_set_timer_callback(uint64_t handle) {
  runtime_event_set_operation((uint16_t)handle);
  return true;
}

_Noreturn void RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_static_initialization_duplicate_fatal(void) {
  runtime_event_recovered_trap();
}

RuntimeEventView RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_resolve_object_id(uint64_t object_id) {
  if (!event_configuration_bound || event_configuration.resolve == NULL) {
    runtime_event_recovered_trap();
  }
  RuntimeEventView event = event_configuration.resolve(
      object_id,
      sizeof(RuntimeEvent),
      event_configuration.context
  );
  if (!runtime_event_object_in_bounds(event)) {
    runtime_event_recovered_trap();
  }
  return event;
}

void RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_assign_signal_state(RuntimeEventView event, bool signaled) {
  if (!runtime_event_object_in_bounds(event)) {
    runtime_event_recovered_trap();
  }
  event.current->signaled = signaled;
  if (!signaled) {
    return;
  }
  if (event.current->auto_reset == 1) {
    if (runtime_event_wake_one_operation(event.current)) {
      event.current->signaled = 0;
    }
    return;
  }
  while (runtime_event_wake_one_operation(event.current)) {
  }
}

RuntimeEventStaticDescriptor *RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_registration_tuple_validate(RuntimeEventRegistrationTuple tuple) {
  if (tuple.type_token != RUNTIME_EVENT_REGISTRATION_TYPE_TOKEN ||
      !runtime_event_descriptor_in_bounds(tuple.current, tuple.lower_bound,
                                          tuple.upper_bound)) {
    runtime_event_recovered_trap();
  }
  return tuple.current;
}

static void
runtime_event_publish_static_handle(
    RuntimeEventStaticDescriptor *descriptor,
    uint16_t handle
) {
  const uintptr_t address = (uintptr_t)descriptor->current;
  const uintptr_t lower = (uintptr_t)descriptor->lower_bound;
  const uintptr_t upper = (uintptr_t)descriptor->upper_bound;

  if (address == 0 || lower == 0 || upper == 0 || address < lower ||
      address >= upper || handle == 0) {
    runtime_event_recovered_trap();
  }
  *descriptor->current = handle;
}

void RUNTIME_EVENT_RECOVERED_ENTRY
runtime_event_initialize_static_records(void) {
  if (blocking_wait_enabled) {
    runtime_event_static_initialization_duplicate_fatal();
  }
  if (!event_configuration_bound) {
    runtime_event_recovered_trap();
  }
  for (size_t index = 0; index < event_configuration.registration_count;
       ++index) {
    RuntimeEventStaticDescriptor *descriptor =
        runtime_event_registration_tuple_validate(
            event_configuration.registrations[index]);
    const uint16_t handle = runtime_event_create_svc(
        descriptor->auto_reset != 0,
        descriptor->initially_signaled != 0
    );
    runtime_event_publish_static_handle(descriptor, handle);
  }
  const uint64_t timestamp = runtime_event_counter_read();
  if (event_configuration.initialization_timestamp != NULL) {
    *event_configuration.initialization_timestamp = timestamp;
  }
  blocking_wait_enabled = true;
}

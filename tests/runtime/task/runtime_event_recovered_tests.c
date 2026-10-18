#include "avp/runtime/task/runtime_event_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  uint16_t next_handle;
  bool create_auto_reset[4];
  bool create_initial_state[4];
  size_t create_count;
  bool wait_results[8];
  size_t wait_result_count;
  size_t wait_count;
  uint16_t wait_handles[8];
  uint64_t wait_deadlines[8];
  uint64_t counter;
  uint64_t microseconds;
  uint64_t tick_scale;
  uint16_t set_handle;
  size_t set_count;
  RuntimeEvent events[2];
  size_t resolve_size;
  uint64_t resolved_object_id;
  size_t wake_successes;
  size_t wake_calls;
} EventProbe;

static EventProbe *active_probe;

uint16_t runtime_event_create(bool auto_reset, bool initially_signaled) {
  (void)auto_reset;
  (void)initially_signaled;
  return active_probe->next_handle++;
}

bool runtime_event_wait(uint16_t handle, uint64_t absolute_deadline) {
  (void)handle;
  (void)absolute_deadline;
  return false;
}

void runtime_event_set(uint16_t handle) {
  active_probe->set_handle = handle;
  ++active_probe->set_count;
}

bool runtime_event_wake_one(RuntimeEvent *event) {
  (void)event;
  return false;
}

uint64_t avp_virtual_counter_read(void) { return active_probe->counter; }

uint64_t runtime_microsecond_time_read(void) {
  return ++active_probe->microseconds;
}

uint64_t avp_microseconds_to_ticks(uint64_t microseconds) {
  return microseconds * active_probe->tick_scale;
}

static uint16_t create_event(bool auto_reset, bool initially_signaled,
                             void *context) {
  EventProbe *probe = context;
  size_t index = probe->create_count++;

  probe->create_auto_reset[index] = auto_reset;
  probe->create_initial_state[index] = initially_signaled;
  return probe->next_handle++;
}

static bool wait_event(uint16_t handle, uint64_t absolute_deadline,
                       void *context) {
  EventProbe *probe = context;
  size_t index = probe->wait_count++;

  probe->wait_handles[index] = handle;
  probe->wait_deadlines[index] = absolute_deadline;
  return index < probe->wait_result_count ? probe->wait_results[index] : false;
}

static void set_event(uint16_t handle, void *context) {
  EventProbe *probe = context;

  probe->set_handle = handle;
  ++probe->set_count;
}

static uint64_t read_counter(void *context) {
  return ((EventProbe *)context)->counter;
}

static uint64_t read_microseconds(void *context) {
  return ++((EventProbe *)context)->microseconds;
}

static uint64_t microseconds_to_ticks(uint64_t microseconds, void *context) {
  return microseconds * ((EventProbe *)context)->tick_scale;
}

static RuntimeEventView resolve_event(uint64_t object_id, size_t object_size,
                                      void *context) {
  EventProbe *probe = context;

  probe->resolved_object_id = object_id;
  probe->resolve_size = object_size;
  return (RuntimeEventView){
      .current = &probe->events[1],
      .lower_bound = &probe->events[0],
      .upper_bound = &probe->events[2],
  };
}

static bool wake_one(RuntimeEvent *event, void *context) {
  EventProbe *probe = context;

  assert(event >= &probe->events[0] && event < &probe->events[2]);
  ++probe->wake_calls;
  if (probe->wake_successes == 0) {
    return false;
  }
  --probe->wake_successes;
  return true;
}

static RuntimeEventRecoveredConfiguration configuration(EventProbe *probe) {
  return (RuntimeEventRecoveredConfiguration){
      .create = create_event,
      .wait = wait_event,
      .set = set_event,
      .counter_read = read_counter,
      .microseconds_read = read_microseconds,
      .microseconds_to_ticks = microseconds_to_ticks,
      .resolve = resolve_event,
      .wake_one = wake_one,
      .context = probe,
  };
}

static void expect_trap(void (*operation)(void)) {
  pid_t child = fork();

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

static void duplicate_initialization(void) {
  runtime_event_initialize_static_records();
}

static void invalid_registration(void) {
  RuntimeEventStaticDescriptor descriptor;
  RuntimeEventRegistrationTuple tuple;

  memset(&descriptor, 0, sizeof(descriptor));
  tuple = (RuntimeEventRegistrationTuple){
      .current = &descriptor,
      .lower_bound = &descriptor,
      .upper_bound = &descriptor + 1,
      .type_token = 0,
  };
  (void)runtime_event_registration_tuple_validate(tuple);
}

static void failed_wait_forever(void) { runtime_event_wait_forever(7); }

static void test_polling_waits(EventProbe *probe) {
  RuntimeEventRecoveredConfiguration config = configuration(probe);

  runtime_event_recovered_configure(&config);
  probe->wait_results[0] = false;
  probe->wait_results[1] = false;
  probe->wait_results[2] = true;
  probe->wait_result_count = 3;
  probe->microseconds = 10;
  assert(runtime_event_wait_with_timeout(9, 5));
  assert(probe->wait_count == 3);
  assert(probe->wait_deadlines[0] == 0);
  assert(probe->wait_deadlines[1] == 0);
  assert(probe->wait_deadlines[2] == 0);

  probe->wait_count = 0;
  probe->wait_result_count = 0;
  probe->microseconds = 1;
  assert(!runtime_event_wait_with_timeout(4, 0));
  assert(probe->wait_count == 1);
}

static void test_static_records_and_blocking_wait(EventProbe *probe) {
  uint16_t handles[2] = {0, 0};
  RuntimeEventStaticDescriptor descriptors[2];
  RuntimeEventRegistrationTuple registrations[2];
  uint64_t timestamp = 0;

  memset(descriptors, 0, sizeof(descriptors));
  for (size_t index = 0; index < 2; ++index) {
    descriptors[index].current = &handles[index];
    descriptors[index].lower_bound = &handles[0];
    descriptors[index].upper_bound = &handles[2];
    registrations[index] = (RuntimeEventRegistrationTuple){
        .current = &descriptors[index],
        .lower_bound = &descriptors[0],
        .upper_bound = &descriptors[2],
        .type_token = RUNTIME_EVENT_REGISTRATION_TYPE_TOKEN,
    };
  }
  descriptors[0].auto_reset = 1;
  descriptors[1].initially_signaled = 1;
  RuntimeEventRecoveredConfiguration config = configuration(probe);
  config.registrations = registrations;
  config.registration_count = 2;
  config.initialization_timestamp = &timestamp;
  runtime_event_recovered_configure(&config);

  probe->next_handle = 0x1200;
  probe->counter = 100;
  probe->tick_scale = 10;
  runtime_event_initialize_static_records();
  assert(runtime_event_blocking_wait_is_enabled());
  assert(handles[0] == 0x1200 && handles[1] == 0x1201);
  assert(probe->create_count == 2);
  assert(probe->create_auto_reset[0]);
  assert(!probe->create_initial_state[0]);
  assert(!probe->create_auto_reset[1]);
  assert(probe->create_initial_state[1]);
  assert(timestamp == 100);

  probe->wait_count = 0;
  probe->wait_result_count = 1;
  probe->wait_results[0] = true;
  assert(runtime_event_wait_with_timeout(0x55, 7));
  assert(probe->wait_handles[0] == 0x55);
  assert(probe->wait_deadlines[0] == 170);

  probe->wait_count = 0;
  runtime_event_wait_forever(0x56);
  assert(probe->wait_deadlines[0] == UINT64_MAX);
  expect_trap(duplicate_initialization);

  probe->wait_count = 0;
  probe->wait_result_count = 1;
  probe->wait_results[0] = false;
  expect_trap(failed_wait_forever);
}

static void test_resolution_and_signal(EventProbe *probe) {
  RuntimeEventRecoveredConfiguration config = configuration(probe);
  RuntimeEventView event;

  runtime_event_recovered_configure(&config);
  event = runtime_event_resolve_object_id(UINT64_C(0x123456789));
  assert(probe->resolved_object_id == UINT64_C(0x123456789));
  assert(probe->resolve_size == sizeof(RuntimeEvent));
  assert(event.current == &probe->events[1]);

  event.current->auto_reset = 1;
  probe->wake_successes = 1;
  runtime_event_assign_signal_state(event, true);
  assert(event.current->signaled == 0);
  assert(probe->wake_calls == 1);

  probe->wake_calls = 0;
  probe->wake_successes = 0;
  runtime_event_assign_signal_state(event, true);
  assert(event.current->signaled == 1);
  assert(probe->wake_calls == 1);

  event.current->auto_reset = 0;
  probe->wake_calls = 0;
  probe->wake_successes = 2;
  runtime_event_assign_signal_state(event, true);
  assert(event.current->signaled == 1);
  assert(probe->wake_calls == 3);

  probe->wake_calls = 0;
  runtime_event_assign_signal_state(event, false);
  assert(event.current->signaled == 0);
  assert(probe->wake_calls == 0);
}

static void test_create_and_timer_callback(EventProbe *probe) {
  RuntimeEventRecoveredConfiguration config = configuration(probe);

  runtime_event_recovered_configure(&config);
  probe->next_handle = 0x4321;
  assert(runtime_event_create_svc(true, false) == 0x4321);
  assert(runtime_event_set_timer_callback(UINT64_C(0x1234abcd)));
  assert(probe->set_count == 1);
  assert(probe->set_handle == 0xabcd);
}

int main(void) {
  EventProbe probe;

  memset(&probe, 0, sizeof(probe));
  active_probe = &probe;
  test_polling_waits(&probe);

  memset(&probe, 0, sizeof(probe));
  active_probe = &probe;
  test_static_records_and_blocking_wait(&probe);

  memset(&probe, 0, sizeof(probe));
  active_probe = &probe;
  test_resolution_and_signal(&probe);

  memset(&probe, 0, sizeof(probe));
  active_probe = &probe;
  test_create_and_timer_callback(&probe);
  expect_trap(invalid_registration);
  return 0;
}

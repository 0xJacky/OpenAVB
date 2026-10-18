#include "avp/runtime/support/runtime_console_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  RuntimeByteRing input_ring;
  RuntimeByteRing output_ring;
  uint8_t input_bytes[0x100];
  uint8_t output_bytes[8];
  uint8_t drained[0x10000];
  uint8_t root[0x10000];
  size_t drained_size;
  size_t root_size;
  uint32_t event_flags;
  uint16_t set_handles[32];
  size_t set_count;
  uint16_t clear_handle;
  unsigned yield_count;
  unsigned lock_depth;
} ConsoleProbe;

static const RuntimeFirebloomType byte_type = {
    .identity = UINT64_C(0x62797465),
    .element_size = 1,
    .bytewise_copyable = true,
};

static RuntimeByteRingControlTuple control_tuple(RuntimeByteRing *ring) {
  return (RuntimeByteRingControlTuple){
      .current = ring,
      .lower_bound = (uintptr_t)ring,
      .upper_bound = (uintptr_t)(ring + 1),
      .cookie = UINT64_C(0x52494e47),
  };
}

static RuntimeFirebloomSpan byte_span(uint8_t *bytes, size_t size) {
  return (RuntimeFirebloomSpan){
      .current = bytes,
      .lower_bound = bytes,
      .upper_bound = bytes + size,
      .type = &byte_type,
  };
}

static uint16_t event_create(uint32_t flags, void *context) {
  ConsoleProbe *probe = context;

  probe->event_flags = flags;
  return UINT16_C(0x1234);
}

static void event_set(uint16_t handle, void *context) {
  ConsoleProbe *probe = context;

  assert(probe->set_count < 32);
  probe->set_handles[probe->set_count++] = handle;
}

static void event_clear(uint16_t handle, void *context) {
  ConsoleProbe *probe = context;

  probe->clear_handle = handle;
}

static void lock_console(void *context) {
  ConsoleProbe *probe = context;

  ++probe->lock_depth;
}

static void unlock_console(void *context) {
  ConsoleProbe *probe = context;

  assert(probe->lock_depth != 0);
  --probe->lock_depth;
}

static void drain_ring(ConsoleProbe *probe) {
  RuntimeByteRing *ring = &probe->output_ring;

  while (ring->produced_count != 0) {
    assert(probe->drained_size < sizeof(probe->drained));
    probe->drained[probe->drained_size++] =
        ((uint8_t *)ring->storage.current)[ring->read_index];
    ring->read_index = (ring->read_index + 1) & ring->mask;
    --ring->produced_count;
  }
}

static void yield_console(void *context) {
  ConsoleProbe *probe = context;

  ++probe->yield_count;
  drain_ring(probe);
}

static size_t root_write(const uint8_t *bytes, size_t size, void *context) {
  ConsoleProbe *probe = context;

  assert(size <= sizeof(probe->root) - probe->root_size);
  memcpy(probe->root + probe->root_size, bytes, size);
  probe->root_size += size;
  return size;
}

static void configure_console(ConsoleProbe *probe, RuntimeConsoleRoute route) {
  const RuntimeByteRingControlTuple output_control =
      control_tuple(&probe->output_ring);
  const RuntimeFirebloomSpan output_storage =
      byte_span(probe->output_bytes, sizeof(probe->output_bytes));
  const RuntimeConsoleConfiguration configuration = {
      .input_control = control_tuple(&probe->input_ring),
      .input_storage =
          byte_span(probe->input_bytes, sizeof(probe->input_bytes)),
      .input_physical_region = {0},
      .output_control = output_control,
      .output_drain_event = UINT16_C(0x5678),
      .route = route,
      .event_create = event_create,
      .event_set = event_set,
      .event_clear = event_clear,
      .yield = yield_console,
      .lock = lock_console,
      .unlock = unlock_console,
      .root_write = root_write,
      .context = probe,
  };

  (void)runtime_byte_ring_initialize(
      output_control, output_storage, (RuntimeByteRingPhysicalRegion){0},
      sizeof(probe->output_bytes), NULL, NULL, NULL, 0);
  runtime_console_configure(&configuration);
}

static ConsoleProbe g_trap_probe;

static void unterminated_cstring(void) {
  uint8_t bytes[4] = {'f', 'a', 'i', 'l'};

  configure_console(&g_trap_probe, RUNTIME_CONSOLE_ROUTE_DISCARD);
  runtime_console_write_cstring(byte_span(bytes, sizeof(bytes)));
}

static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  int status;

  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void test_input_and_events(void) {
  ConsoleProbe probe;

  memset(&probe, 0, sizeof(probe));
  configure_console(&probe, RUNTIME_CONSOLE_ROUTE_BUFFERED);
  runtime_console_input_initialize();
  assert(probe.event_flags == 0x0b);
  assert(probe.input_ring.capacity == 0x100);
  assert(probe.input_ring.mask == 0xff);
  assert(probe.input_ring.push_notification ==
         runtime_console_input_ring_push_notify);
  assert(probe.input_ring.pop_notification ==
         runtime_console_input_ring_pop_notify);
  runtime_console_input_ring_push_notify(NULL);
  runtime_console_input_ring_pop_notify(NULL);
  assert(probe.set_handles[0] == UINT16_C(0x1234));
  assert(probe.clear_handle == UINT16_C(0x1234));
  runtime_console_output_drain_signal();
  assert(probe.set_handles[1] == UINT16_C(0x5678));
}

static void test_buffered_crlf_and_full_ring(void) {
  ConsoleProbe probe;
  uint8_t message[] = "alpha\nbeta\ngamma";
  static const uint8_t expected[] = "alpha\r\nbeta\r\ngamma";

  memset(&probe, 0, sizeof(probe));
  configure_console(&probe, RUNTIME_CONSOLE_ROUTE_BUFFERED);
  runtime_console_write_span(byte_span(message, sizeof(message)),
                             sizeof(message) - 1);
  drain_ring(&probe);
  assert(probe.lock_depth == 0);
  assert(probe.yield_count >= 1);
  assert(probe.drained_size == sizeof(expected) - 1);
  assert(memcmp(probe.drained, expected, sizeof(expected) - 1) == 0);
  assert(probe.set_handles[probe.set_count - 1] == UINT16_C(0x5678));
}

static void test_root_chunking_and_cstring_status(void) {
  ConsoleProbe probe;
  static uint8_t message[0x4011];

  memset(&probe, 0, sizeof(probe));
  for (size_t index = 0; index < sizeof(message); ++index) {
    message[index] = (uint8_t)(index * 17 + 3);
  }
  configure_console(&probe, RUNTIME_CONSOLE_ROUTE_ROOT_RPC);
  runtime_console_write_span(byte_span(message, sizeof(message)),
                             sizeof(message));
  assert(probe.root_size == sizeof(message));
  assert(memcmp(probe.root, message, sizeof(message)) == 0);

  probe.root_size = 0;
  {
    uint8_t cstring[] = "status";
    assert(runtime_console_write_cstring_status(
               byte_span(cstring, sizeof(cstring))) == 0);
    assert(probe.root_size == sizeof(cstring) - 1);
    assert(memcmp(probe.root, cstring, sizeof(cstring) - 1) == 0);
  }
  expect_trap(unterminated_cstring);
}

static void test_discard_and_outliners(void) {
  ConsoleProbe probe;
  uint8_t bytes[] = {1, 2, 3, 4};
  uint8_t state[0x90];
  RuntimeFirebloomSpan span = byte_span(bytes, sizeof(bytes));
  RuntimeFirebloomSpan result;

  memset(&probe, 0, sizeof(probe));
  memset(state, 0, sizeof(state));
  configure_console(&probe, RUNTIME_CONSOLE_ROUTE_DISCARD);
  runtime_console_write_span(span, sizeof(bytes));
  assert(probe.root_size == 0);
  assert(probe.output_ring.produced_count == 0);
  result = runtime_console_outlined_save_span_x22_x19(span);
  assert(result.current == span.current);
  result = runtime_console_outlined_restore_span_x22_x19(result);
  assert(result.upper_bound == span.upper_bound);
  result =
      runtime_console_outlined_restore_span_bounds_x21_x19(bytes + 1, span);
  assert(result.current == bytes + 1);
  assert(result.lower_bound == span.lower_bound);
  result = runtime_console_outlined_lock_tuple_x26(state);
  assert(result.current == state + 0x68);
  assert(result.lower_bound == state + 0x68);
  assert(result.upper_bound == state + 0x90);
  assert(result.type == &runtime_console_lock_type);
  result = runtime_console_outlined_lock_tuple_x25(state, &byte_type);
  assert(result.type == &byte_type);
}

int main(void) {
  test_input_and_events();
  test_buffered_crlf_and_full_ring();
  test_root_chunking_and_cstring_status();
  test_discard_and_outliners();
  return 0;
}

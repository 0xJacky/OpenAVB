#include "avp/runtime/support/runtime_core_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  char source[64];
  char message[RUNTIME_CORE_FATAL_BUFFER_SIZE];
  uintptr_t context_current;
  size_t rendered_length;
  int formatted_length;
  uint32_t action;
  uint32_t depth;
  bool bounded_format;
} FatalProbe;

static FatalProbe *active_probe;

static void copy_cstring(char *destination, size_t capacity,
                         const char *source) {
  size_t index = 0;

  assert(capacity != 0);
  while (index + 1 < capacity && source[index] != '\0') {
    destination[index] = source[index];
    ++index;
  }
  destination[index] = '\0';
}

static void publish_fatal(const RuntimeCoreFatalPublication *publication,
                          void *context) {
  FatalProbe *probe = context;

  copy_cstring(probe->source, sizeof(probe->source), publication->source);
  memcpy(probe->message, publication->message.current, sizeof(probe->message));
  probe->context_current = (uintptr_t)publication->context.current;
  probe->rendered_length = publication->rendered_length;
  probe->formatted_length = publication->formatted_length;
  probe->bounded_format = publication->bounded_format;
  probe->action = 1;
  __builtin_trap();
}

static void halt_fatal(uint32_t depth, void *context) {
  FatalProbe *probe = context;

  probe->action = 3;
  probe->depth = depth;
  __builtin_trap();
}

static void double_panic_to_triple(const char *source, void *context) {
  FatalProbe *probe = context;

  copy_cstring(probe->source, sizeof(probe->source), source);
  probe->action = 2;
  runtime_fatal_nesting_enter("triple-panic");
}

_Noreturn void runtime_fatal(void) {
  active_probe->action = 4;
  _exit(77);
}

static RuntimeCoreConfiguration configuration(FatalProbe *probe) {
  return (RuntimeCoreConfiguration){
      .publish = publish_fatal,
      .double_panic = double_panic_to_triple,
      .halt = halt_fatal,
      .context = probe,
  };
}

static _Noreturn void invoke_vformat(const char *source, bool bounded_format,
                                     const char *format, ...) {
  va_list arguments;

  va_start(arguments, format);
  runtime_fatal_vformat(source, format, bounded_format, arguments);
  va_end(arguments);
  __builtin_unreachable();
}

static _Noreturn void invoke_render(const RuntimeFirebloomSpan *context,
                                    const char *source, const char *format,
                                    ...) {
  va_list arguments;

  va_start(arguments, format);
  runtime_fatal_render_and_publish(context, source, format, false, arguments);
  va_end(arguments);
  __builtin_unreachable();
}

static void expect_signal(pid_t child) {
  int status;

  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void test_vformat_and_default_context(FatalProbe *probe) {
  const pid_t child = fork();

  assert(child >= 0);
  if (child == 0) {
    const RuntimeCoreConfiguration config = configuration(probe);
    runtime_core_configure(&config);
    invoke_vformat("runtime-core", false, "value=%08x/%s", 0x2a, "ok");
  }
  expect_signal(child);
  assert(probe->action == 1);
  assert(strcmp(probe->source, "runtime-core") == 0);
  assert(strcmp(probe->message, "value=0000002a/ok") == 0);
  assert(probe->rendered_length == strlen(probe->message));
  assert(probe->formatted_length == (int)strlen(probe->message));
  assert(probe->context_current != 0);
  assert(!probe->bounded_format);
}

static void test_explicit_context(FatalProbe *probe) {
  uint8_t context_byte = 0x5a;
  const RuntimeFirebloomSpan context = {
      .current = &context_byte,
      .lower_bound = &context_byte,
      .upper_bound = &context_byte + 1,
      .type = &runtime_core_fatal_byte_type,
  };
  const pid_t child = fork();

  assert(child >= 0);
  if (child == 0) {
    const RuntimeCoreConfiguration config = configuration(probe);
    runtime_core_configure(&config);
    invoke_render(&context, "explicit", "hex=%#x", 0x44);
  }
  expect_signal(child);
  assert(probe->action == 1);
  assert(probe->context_current == (uintptr_t)&context_byte);
  assert(strcmp(probe->message, "hex=0x44") == 0);
}

static void test_bounded_truncation(FatalProbe *probe) {
  char input[0x900];
  const pid_t child = fork();

  memset(input, 'Q', sizeof(input));
  input[sizeof(input) - 1] = '\0';
  assert(child >= 0);
  if (child == 0) {
    const RuntimeCoreConfiguration config = configuration(probe);
    runtime_core_configure(&config);
    runtime_fatal_format_bounded("bounded", "%s", input);
  }
  expect_signal(child);
  assert(probe->action == 1);
  assert(probe->bounded_format);
  assert(probe->rendered_length == RUNTIME_CORE_FATAL_BUFFER_SIZE - 1);
  assert(probe->message[RUNTIME_CORE_FATAL_BUFFER_SIZE - 1] == '\0');
  assert(probe->formatted_length == (int)strlen(input));
}

static void test_recursive_fatal_routes_to_halt(FatalProbe *probe) {
  const pid_t child = fork();

  assert(child >= 0);
  if (child == 0) {
    const RuntimeCoreConfiguration config = configuration(probe);
    runtime_core_configure(&config);
    runtime_fatal_nesting_enter("first");
    runtime_fatal_nesting_enter("second");
    _exit(0);
  }
  expect_signal(child);
  assert(probe->action == 3);
  assert(probe->depth == 3);
  assert(strcmp(probe->source, "second") == 0);
}

static void test_abort_adapter(FatalProbe *probe) {
  const pid_t child = fork();
  int status;

  assert(child >= 0);
  if (child == 0) {
    active_probe = probe;
    runtime_abort_fatal();
  }
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 77);
  assert(probe->action == 4);
}

static void test_outlined_argument_state(void) {
  const RuntimeFatalFormatArguments arguments =
      runtime_fatal_outlined_prepare_format_arguments(
          "source", "format=%u", UINT64_C(0x123456789abcdef0));

  assert(arguments.context.current == NULL);
  assert(arguments.context.lower_bound == NULL);
  assert(arguments.context.upper_bound == NULL);
  assert(arguments.context.type == NULL);
  assert(strcmp(arguments.source, "source") == 0);
  assert(strcmp(arguments.format, "format=%u") == 0);
  assert(arguments.saved_return_address == UINT64_C(0x123456789abcdef0));
}

int main(void) {
  FatalProbe *probe = mmap(NULL, sizeof(*probe), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  assert(probe != MAP_FAILED);
  test_vformat_and_default_context(probe);
  memset(probe, 0, sizeof(*probe));
  test_explicit_context(probe);
  memset(probe, 0, sizeof(*probe));
  test_bounded_truncation(probe);
  memset(probe, 0, sizeof(*probe));
  test_recursive_fatal_routes_to_halt(probe);
  memset(probe, 0, sizeof(*probe));
  test_abort_adapter(probe);
  test_outlined_argument_state();
  assert(munmap(probe, sizeof(*probe)) == 0);
  return 0;
}

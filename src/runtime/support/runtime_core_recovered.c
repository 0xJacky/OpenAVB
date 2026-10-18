#include "avp/runtime/support/runtime_core_recovered.h"

#include "avp/runtime/support/runtime_format_compat.h"

const RuntimeFirebloomType runtime_core_fatal_byte_type = {
    .identity = UINT64_C(0x666174616c627974),
    .element_size = 1,
    .bytewise_copyable = true,
};

static RuntimeCoreConfiguration fatal_configuration;
static uint32_t fatal_nesting_depth;
static uint8_t default_context_storage[sizeof(RuntimeFirebloomSpan)];

_Noreturn void runtime_fatal(void);

static _Noreturn void runtime_core_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static RuntimeFirebloomSpan runtime_core_default_context(void) {
  /* The renderer exposes its 32-byte empty tuple as a valid span. */
  return (RuntimeFirebloomSpan){
      .current = default_context_storage,
      .lower_bound = default_context_storage,
      .upper_bound = default_context_storage + sizeof(default_context_storage),
      .type = &runtime_core_fatal_byte_type,
  };
}

static size_t runtime_core_bounded_length(const char *string, size_t capacity) {
  size_t length = 0;

  while (length < capacity && string[length] != '\0') {
    ++length;
  }
  return length;
}

void runtime_core_configure(const RuntimeCoreConfiguration *configuration) {
  if (configuration == NULL) {
    runtime_core_trap();
  }
  fatal_configuration = *configuration;
  fatal_nesting_depth = 0;
}

void RUNTIME_CORE_RECOVERED_ENTRY
runtime_fatal_nesting_enter(const char *source) {
  ++fatal_nesting_depth;
  if (fatal_nesting_depth >= 3) {
    if (fatal_configuration.halt != NULL) {
      fatal_configuration.halt(
          fatal_nesting_depth,
          fatal_configuration.context
      );
      runtime_core_trap();
    }
    runtime_fatal();
  }
  if (fatal_nesting_depth == 2) {
    if (fatal_configuration.double_panic != NULL) {
      fatal_configuration.double_panic(source, fatal_configuration.context);
      runtime_core_trap();
    }
    runtime_fatal();
  }
}

_Noreturn void RUNTIME_CORE_RECOVERED_ENTRY runtime_fatal_render_and_publish(
    const RuntimeFirebloomSpan *context,
    const char *source,
    const char *format,
    bool bounded_format,
    va_list arguments
) {
  char message[RUNTIME_CORE_FATAL_BUFFER_SIZE] = {0};
  RuntimeFirebloomSpan publication_context;
  int formatted_length;

  if (context == NULL || source == NULL || format == NULL) {
    runtime_core_trap();
  }

  runtime_fatal_nesting_enter(source);
  if (bounded_format) {
    formatted_length = runtime_firebloom_vsnprintf(
        message,
        sizeof(message),
        format,
        arguments
    );
  } else {
    formatted_length =
        runtime_vsnprintf(message, sizeof(message), format, arguments);
  }

  publication_context =
      context->current == NULL ? runtime_core_default_context() : *context;
  const RuntimeCoreFatalPublication publication = {
      .source = source,
      .message =
          {
              .current = message,
              .lower_bound = message,
              .upper_bound = message + sizeof(message),
              .type = &runtime_core_fatal_byte_type,
          },
      .context = publication_context,
      .rendered_length = runtime_core_bounded_length(message, sizeof(message)),
      .formatted_length = formatted_length,
      .bounded_format = bounded_format,
  };

  if (fatal_configuration.publish != NULL) {
    fatal_configuration.publish(&publication, fatal_configuration.context);
    runtime_core_trap();
  }
  runtime_fatal();
}

_Noreturn void RUNTIME_CORE_RECOVERED_ENTRY
runtime_fatal_vformat(
    const char *source,
    const char *format,
    bool bounded_format,
    va_list arguments
) {
  const RuntimeFirebloomSpan empty_context = {0};

  runtime_fatal_render_and_publish(
      &empty_context,
      source,
      format,
      bounded_format,
      arguments
  );
}

_Noreturn void RUNTIME_CORE_RECOVERED_ENTRY
runtime_fatal_format_bounded(const char *source, const char *format, ...) {
  va_list arguments;

  va_start(arguments, format);
  runtime_fatal_vformat(source, format, true, arguments);
  va_end(arguments);
  __builtin_unreachable();
}

_Noreturn void RUNTIME_CORE_RECOVERED_ENTRY runtime_abort_fatal(void) {
  runtime_fatal();
}

RuntimeFatalFormatArguments RUNTIME_CORE_RECOVERED_ENTRY
runtime_fatal_outlined_prepare_format_arguments(
    const char *source,
    const char *format,
    uintptr_t saved_return_address
) {
  return (RuntimeFatalFormatArguments){
      .context = {0},
      .source = source,
      .format = format,
      .saved_return_address = saved_return_address,
  };
}

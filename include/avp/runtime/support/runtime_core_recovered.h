#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_CORE_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_CORE_RECOVERED_ENTRY __attribute__((used))
#endif

enum {
  RUNTIME_CORE_FATAL_BUFFER_SIZE = 0x800,
};

typedef struct {
  const char *source;
  RuntimeFirebloomSpan message;
  RuntimeFirebloomSpan context;
  size_t rendered_length;
  int formatted_length;
  bool bounded_format;
} RuntimeCoreFatalPublication;

typedef void (*RuntimeCoreFatalPublish)(
  const RuntimeCoreFatalPublication *publication,
  void *context
);
typedef void (*RuntimeCoreDoublePanic)(const char *source, void *context);
typedef void (*RuntimeCoreHalt)(uint32_t nesting_depth, void *context);

typedef struct {
  RuntimeCoreFatalPublish publish;
  RuntimeCoreDoublePanic double_panic;
  RuntimeCoreHalt halt;
  void *context;
} RuntimeCoreConfiguration;

typedef struct {
  RuntimeFirebloomSpan context;
  const char *source;
  const char *format;
  uintptr_t saved_return_address;
} RuntimeFatalFormatArguments;

extern const RuntimeFirebloomType runtime_core_fatal_byte_type;

void runtime_core_configure(const RuntimeCoreConfiguration *configuration);

RUNTIME_CORE_RECOVERED_ENTRY _Noreturn void
runtime_fatal_vformat(
  const char *source,
  const char *format,
  bool bounded_format,
  va_list arguments
);
RUNTIME_CORE_RECOVERED_ENTRY _Noreturn void
runtime_fatal_render_and_publish(
  const RuntimeFirebloomSpan *context,
  const char *source,
  const char *format,
  bool bounded_format,
  va_list arguments
);
RUNTIME_CORE_RECOVERED_ENTRY _Noreturn void
runtime_fatal_format_bounded(const char *source, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
RUNTIME_CORE_RECOVERED_ENTRY _Noreturn void runtime_abort_fatal(void);
RUNTIME_CORE_RECOVERED_ENTRY void
runtime_fatal_nesting_enter(const char *source);
RUNTIME_CORE_RECOVERED_ENTRY RuntimeFatalFormatArguments
runtime_fatal_outlined_prepare_format_arguments(
  const char *source,
  const char *format,
  uintptr_t saved_return_address
);

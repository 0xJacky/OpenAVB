#pragma once

#include <stdint.h>


typedef void (*RuntimeLogFunction)(
    uint32_t level,
    const char *format,
    ...
);
typedef void (*RuntimeSecondaryLogFunction)(const char *format, ...);

RuntimeLogFunction runtime_log_set_function(RuntimeLogFunction function);
RuntimeLogFunction runtime_log_get_function(void);
__attribute__((used, retain)) void runtime_log_set_secondary_function(
    RuntimeSecondaryLogFunction function
);
RuntimeSecondaryLogFunction runtime_log_get_secondary_function(void);
__attribute__((used, retain)) uint64_t runtime_physical_memory_source_token(void);

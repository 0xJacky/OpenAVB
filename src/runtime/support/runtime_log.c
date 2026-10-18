#include "avp/runtime/support/runtime_log.h"


#if defined(__ELF__)
#define RUNTIME_LOG_RECOVERED \
    __attribute__((noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_LOG_RECOVERED __attribute__((noinline))
#endif

static RuntimeLogFunction log_function;
static RuntimeSecondaryLogFunction secondary_log_function;

RUNTIME_LOG_RECOVERED
RuntimeLogFunction runtime_log_set_function(RuntimeLogFunction function) {
    log_function = function;
    return function;
}

RUNTIME_LOG_RECOVERED
RuntimeLogFunction runtime_log_get_function(void) {
    return log_function;
}

RUNTIME_LOG_RECOVERED
void runtime_log_set_secondary_function(
    RuntimeSecondaryLogFunction function
) {
    secondary_log_function = function;
}

RUNTIME_LOG_RECOVERED
RuntimeSecondaryLogFunction runtime_log_get_secondary_function(void) {
    return secondary_log_function;
}

RUNTIME_LOG_RECOVERED
uint64_t runtime_physical_memory_source_token(void) {
    return UINT64_C(0x019c1d143824af8e);
}

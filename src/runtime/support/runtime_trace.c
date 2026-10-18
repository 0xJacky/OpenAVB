#include "avp/runtime/support/runtime_trace.h"

#include "avp/runtime/support/time.h"

#if defined(__ELF__)
#define RUNTIME_TRACE_STATE __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_TRACE_STATE
#endif

static RuntimeTraceRecord trace_records[RUNTIME_TRACE_CAPACITY]
    RUNTIME_TRACE_STATE;
static size_t trace_count RUNTIME_TRACE_STATE;

void runtime_trace_append_timestamped_event(
    uint64_t callsite,
    uint32_t event_code,
    uint32_t detail
) {
    if (trace_count == RUNTIME_TRACE_CAPACITY) {
        return;
    }
    RuntimeTraceRecord *record = &trace_records[trace_count++];
    record->timestamp = avp_time_counter();
    record->callsite = callsite;
    record->event_code = event_code;
    record->detail = detail;
}

void runtime_trace_emit_callsite_event(
    uint32_t event_code,
    uint32_t detail
) {
    uintptr_t callsite = (uintptr_t)__builtin_return_address(0);
#if defined(__ARM_FEATURE_PAUTH)
    __asm__ volatile("xpaci %0" : "+r"(callsite));
#endif
    runtime_trace_append_timestamped_event(callsite, event_code, detail);
}

void runtime_trace_reset(void) {
    trace_count = 0;
}

size_t runtime_trace_count(void) {
    return trace_count;
}

const RuntimeTraceRecord *runtime_trace_records(void) {
    return trace_records;
}

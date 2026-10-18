#pragma once

#include <stddef.h>
#include <stdint.h>

#define RUNTIME_TRACE_CAPACITY 0x2aau
#define RUNTIME_TRACE_PUBLIC __attribute__((used, retain))

typedef struct {
    uint64_t timestamp;
    uint64_t callsite;
    uint32_t event_code;
    uint32_t detail;
} RuntimeTraceRecord;

_Static_assert(sizeof(RuntimeTraceRecord) == 24, "runtime trace record size");

RUNTIME_TRACE_PUBLIC void runtime_trace_emit_callsite_event(
    uint32_t event_code,
    uint32_t detail
);
RUNTIME_TRACE_PUBLIC void runtime_trace_append_timestamped_event(
    uint64_t callsite,
    uint32_t event_code,
    uint32_t detail
);
void runtime_trace_reset(void);
size_t runtime_trace_count(void);
const RuntimeTraceRecord *runtime_trace_records(void);

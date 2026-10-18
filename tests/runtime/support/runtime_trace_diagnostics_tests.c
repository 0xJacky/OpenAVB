#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/support/runtime_log.h"
#include "avp/runtime/support/runtime_trace.h"

#include <assert.h>
#include <stdio.h>

static uint64_t counter;

uint64_t avp_time_counter(void) {
    return ++counter;
}

static void primary_log(uint32_t level, const char *format, ...) {
    (void)level;
    (void)format;
}

static void secondary_log(const char *format, ...) {
    (void)format;
}

int main(void) {
    assert(runtime_log_set_function(primary_log) == primary_log);
    assert(runtime_log_get_function() == primary_log);
    runtime_log_set_secondary_function(secondary_log);
    assert(runtime_log_get_secondary_function() == secondary_log);
    assert(runtime_physical_memory_source_token() ==
        UINT64_C(0x019c1d143824af8e));

    assert(pvpanic_select_panic_event(0) == 0);
    assert(pvpanic_select_panic_event(1) == 1);
    assert(pvpanic_select_panic_event(2) == 2);
    assert(pvpanic_select_panic_event(3) == 2);

    runtime_trace_reset();
    runtime_trace_append_timestamped_event(0x1234, 0x4567, 0x89ab);
    assert(runtime_trace_count() == 1);
    const RuntimeTraceRecord *records = runtime_trace_records();
    assert(records[0].timestamp == 1);
    assert(records[0].callsite == 0x1234);
    assert(records[0].event_code == 0x4567);
    assert(records[0].detail == 0x89ab);

    runtime_trace_emit_callsite_event(0xaabb, 0xccdd);
    assert(runtime_trace_count() == 2);
    assert(records[1].timestamp == 2);
    assert(records[1].callsite != 0);
    assert(records[1].event_code == 0xaabb);
    assert(records[1].detail == 0xccdd);
    puts("runtime_trace_diagnostics_tests: ok");
}

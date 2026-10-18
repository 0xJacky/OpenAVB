#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef struct {
    void *context;
    uint64_t (*read_counter)(void *context);
    uint64_t (*counter_frequency)(void *context);
    bool (*should_skip_delay)(void *context);
    void (*wait_iteration)(void *context);
} RuntimeTimePlatform;

#define RUNTIME_TIME_PUBLIC __attribute__((used, retain))

void runtime_time_bind_platform(const RuntimeTimePlatform *platform);
RUNTIME_TIME_PUBLIC uint64_t avp_virtual_counter_read(void);
RUNTIME_TIME_PUBLIC uint64_t avp_counter_frequency(void);
RUNTIME_TIME_PUBLIC uint64_t avp_counter_ticks_to_microseconds(uint64_t ticks);
RUNTIME_TIME_PUBLIC uint64_t avp_counter_ticks_per_microsecond(void);
RUNTIME_TIME_PUBLIC uint64_t avp_microseconds_to_ticks(uint64_t microseconds);
RUNTIME_TIME_PUBLIC uint64_t avp_virtual_counter_read_thunk(void);
RUNTIME_TIME_PUBLIC uint64_t j_avp_virtual_counter_read_clone(void);
RUNTIME_TIME_PUBLIC uint64_t runtime_microsecond_time_read(void);
RUNTIME_TIME_PUBLIC bool runtime_microsecond_interval_elapsed(
    uint64_t start_microseconds,
    uint64_t duration_microseconds
);
RUNTIME_TIME_PUBLIC void runtime_delay_microseconds(uint64_t microseconds);
RUNTIME_TIME_PUBLIC void runtime_conditional_delay_microseconds(
    uint64_t microseconds
);

#include "avp/runtime/support/runtime_time.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


typedef struct {
    uint64_t counter;
    uint64_t step;
    uint64_t frequency;
    uint32_t read_count;
    uint32_t wait_count;
    uint32_t condition_count;
    bool skip_delay;
} MockTime;

static uint64_t mock_read_counter(void *context) {
    MockTime *time = context;
    const uint64_t result = time->counter;
    time->counter += time->step;
    ++time->read_count;
    return result;
}

static uint64_t mock_counter_frequency(void *context) {
    return ((MockTime *)context)->frequency;
}

static bool mock_should_skip_delay(void *context) {
    MockTime *time = context;
    ++time->condition_count;
    return time->skip_delay;
}

static void mock_wait_iteration(void *context) {
    ++((MockTime *)context)->wait_count;
}

static void test_conversions(MockTime *time) {
    assert(avp_counter_frequency() == UINT64_C(10000000));
    assert(avp_counter_ticks_per_microsecond() == 10);
    assert(avp_counter_ticks_to_microseconds(1234) == 123);
    assert(avp_microseconds_to_ticks(321) == 3210);

    time->counter = 900;
    time->step = 0;
    assert(avp_virtual_counter_read() == 900);
    assert(avp_virtual_counter_read_thunk() == 900);
    assert(j_avp_virtual_counter_read_clone() == 900);
    assert(runtime_microsecond_time_read() == 90);
}

static void test_elapsed_wraparound(MockTime *time) {
    time->step = 0;
    time->counter = 1500;
    assert(runtime_microsecond_interval_elapsed(100, 50));
    assert(!runtime_microsecond_interval_elapsed(101, 50));

    time->counter = 20;
    const uint64_t wrapped_start = UINT64_MAX - 3;
    assert(runtime_microsecond_interval_elapsed(wrapped_start, 5));
}

static void test_delays(MockTime *time) {
    time->counter = 0;
    time->step = 5;
    time->wait_count = 0;
    runtime_delay_microseconds(3);
    assert(time->wait_count == 5);
    assert(time->counter == 35);

    const uint32_t reads_before_skip = time->read_count;
    time->skip_delay = true;
    runtime_conditional_delay_microseconds(10);
    assert(time->read_count == reads_before_skip);

    time->skip_delay = false;
    time->counter = 0;
    time->step = 10;
    time->wait_count = 0;
    runtime_conditional_delay_microseconds(2);
    assert(time->wait_count == 1);
    assert(time->condition_count == 2);
}

int main(void) {
    MockTime time = {
        .frequency = UINT64_C(10000000),
    };
    const RuntimeTimePlatform platform = {
        .context = &time,
        .read_counter = mock_read_counter,
        .counter_frequency = mock_counter_frequency,
        .should_skip_delay = mock_should_skip_delay,
        .wait_iteration = mock_wait_iteration,
    };
    runtime_time_bind_platform(&platform);
    test_conversions(&time);
    test_elapsed_wraparound(&time);
    test_delays(&time);
    puts("runtime recovered time tests passed");
    return 0;
}

#include "avp/runtime/support/time.h"


static bool timeout_armed;
static uint64_t counter;

void runtime_transport_test_clock_arm(void) {
    timeout_armed = true;
}

uint64_t avp_time_counter(void) {
    return ++counter;
}

uint64_t avp_time_microseconds(void) {
    return counter;
}

uint64_t avp_time_microseconds_to_ticks(uint64_t microseconds) {
    return microseconds;
}

bool avp_time_has_elapsed(uint64_t start, uint64_t duration) {
    (void)start;
    (void)duration;
    return timeout_armed;
}

void avp_time_delay_microseconds(uint64_t microseconds) {
    counter += microseconds;
}

void avp_time_wait_until_microseconds(uint64_t deadline) {
    counter = deadline;
}
